/*
 * Copyright (C) 2020 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#import "config.h"
#import "WebViewVisualIdentificationOverlay.h"

#if PLATFORM(COCOA)

#import "Color.h"
#import "WebCoreCALayerExtras.h"
#import <CoreText/CoreText.h>
#import <wtf/WeakObjCPtr.h>

#import <pal/ios/UIKitSoftLink.h>

static void *boundsObservationContext = &boundsObservationContext;

@interface WebViewVisualIdentificationOverlay () <CALayerDelegate>
@end

const void* const webViewVisualIdentificationOverlayKey = &webViewVisualIdentificationOverlayKey;

@implementation WebViewVisualIdentificationOverlay {
    RetainPtr<PlatformView> _view;

    RetainPtr<CALayer> _layer;
    RetainPtr<NSString> _kind;
}

+ (BOOL)shouldIdentifyWebViews
{
    static Optional<BOOL> shouldIdentifyWebViews;
    if (!shouldIdentifyWebViews)
        shouldIdentifyWebViews = [[NSUserDefaults standardUserDefaults] boolForKey:@"WebKitDebugIdentifyWebViews"];
    return *shouldIdentifyWebViews;
}

+ (void)installForWebViewIfNeeded:(PlatformView *)view kind:(NSString *)kind deprecated:(BOOL)isDeprecated
{
    if (![self shouldIdentifyWebViews])
        return;
    auto overlay = adoptNS([[WebViewVisualIdentificationOverlay alloc] initWithWebView:view kind:kind deprecated:isDeprecated]);
    objc_setAssociatedObject(self, webViewVisualIdentificationOverlayKey, overlay.get(), OBJC_ASSOCIATION_RETAIN_NONATOMIC);
}

- (instancetype)initWithWebView:(PlatformView *)webView kind:(NSString *)kind deprecated:(BOOL)isDeprecated
{
    self = [super init];
    if (!self)
        return nil;

    _kind = kind;

#if USE(APPKIT)
    _view = adoptNS([[NSView alloc] initWithFrame:webView.bounds]);
    [_view setWantsLayer:YES];
    [_view setAutoresizingMask:NSViewWidthSizable | NSViewHeightSizable];
#else
    _view = adoptNS([PAL::allocUIViewInstance() initWithFrame:webView.bounds]);
    [_view setUserInteractionEnabled:NO];
    [_view setAutoresizingMask:UIViewAutoresizingFlexibleWidth | UIViewAutoresizingFlexibleHeight];
#endif
    [webView addSubview:_view.get()];

#if PLATFORM(MACCATALYST)
    _kind = [_kind stringByAppendingString:@" (macCatalyst)"];
#endif

    _layer = adoptNS([[CATiledLayer alloc] init]);
    [_layer setName:@"WebViewVisualIdentificationOverlay"];
    [_layer setFrame:CGRectMake(0, 0, [_view bounds].size.width, [_view bounds].size.height)];
    auto viewColor = isDeprecated ? WebCore::Color::red.colorWithAlpha(50) : WebCore::Color::blue.colorWithAlpha(32);
    [_layer setBackgroundColor:cachedCGColor(viewColor)];
    [_layer setZPosition:999];
    [_layer setDelegate:self];
    [_layer web_disableAllActions];
    [[_view layer] addSublayer:_layer.get()];

    [[_view layer] addObserver:self forKeyPath:@"bounds" options:0 context:boundsObservationContext];

    return self;
}

- (void)dealloc
{
    [[_view layer] removeObserver:self forKeyPath:@"bounds" context:boundsObservationContext];

    [super dealloc];
}

- (void)observeValueForKeyPath:(NSString *)keyPath ofObject:(id)object change:(NSDictionary<NSKeyValueChangeKey, id> *)change context:(void *)context
{
    UNUSED_PARAM(keyPath);
    UNUSED_PARAM(object);
    UNUSED_PARAM(change);
    if (context == boundsObservationContext) {
        [_layer setFrame:CGRectMake(0, 0, [_view bounds].size.width, [_view bounds].size.height)];
        [_layer setNeedsDisplay];
    }
}

static CTFontRef identificationFont()
{
    auto matrix = CGAffineTransformIdentity;
    return (CTFontRef)CFAutorelease(CTFontCreateWithName(CFSTR("Helvetica"), 20, &matrix));
}

constexpr CGFloat horizontalMargin = 15;
constexpr CGFloat verticalMargin = 5;

static void drawPattern(void *overlayPtr, CGContextRef ctx)
{
    WebViewVisualIdentificationOverlay *overlay = (WebViewVisualIdentificationOverlay *)overlayPtr;

    auto attributes = @{
        (id)kCTFontAttributeName : (id)identificationFont(),
        (id)kCTForegroundColorFromContextAttributeName : @YES
    };
    auto attributedString = adoptCF(CFAttributedStringCreate(kCFAllocatorDefault, (__bridge CFStringRef)overlay->_kind.get(), (__bridge CFDictionaryRef)attributes));
    auto line = adoptCF(CTLineCreateWithAttributedString(attributedString.get()));

    CGSize textSize = [overlay->_kind sizeWithAttributes:attributes];

#if PLATFORM(IOS_FAMILY)
    CGContextScaleCTM(ctx, 1, -1);
    CGContextTranslateCTM(ctx, 0, -(textSize.height + verticalMargin) * 2);
#endif

    CGContextSetTextDrawingMode(ctx, kCGTextFill);

    CGContextSetTextPosition(ctx, 0, 0);
    CGContextSetFillColorWithColor(ctx, cachedCGColor(WebCore::Color::black));
    CTLineDraw(line.get(), ctx);

    CGContextSetTextPosition(ctx, 0, textSize.height + 5);
    CGContextSetFillColorWithColor(ctx, cachedCGColor(WebCore::Color::white));
    CTLineDraw(line.get(), ctx);
}

- (void)drawLayer:(CALayer *)layer inContext:(CGContextRef)ctx
{
    CGPatternCallbacks callbacks = { 0, &drawPattern, nullptr };
    auto patternSpace = adoptCF(CGColorSpaceCreatePattern(nullptr));
    CGContextSetFillColorSpace(ctx, patternSpace.get());

    CGSize textSize = [_kind sizeWithAttributes:@{ (id)kCTFontAttributeName : (id)identificationFont() }];
    CGSize patternSize = CGSizeMake(textSize.width + horizontalMargin, (textSize.height + verticalMargin) * 2);
    auto pattern = adoptCF(CGPatternCreate(self, layer.bounds, CGAffineTransformMakeRotation(M_PI_4), patternSize.width, patternSize.height, kCGPatternTilingNoDistortion, true, &callbacks));
    CGFloat alpha = 0.5;
    CGContextSetFillPattern(ctx, pattern.get(), &alpha);

    CGContextFillRect(ctx, layer.bounds);
}

@end

#endif // PLATFORM(COCOA)
