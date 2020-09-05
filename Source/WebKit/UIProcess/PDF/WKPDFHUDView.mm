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
#import "WKPDFHUDView.h"

#if ENABLE(UI_PROCESS_PDF_HUD)

#import "WebPageProxy.h"
#import <QuartzCore/CATransaction.h>
#import <pal/spi/cocoa/QuartzCoreSPI.h>
#import <pal/spi/mac/NSImageSPI.h>
#import <wtf/WeakObjCPtr.h>

//  The HUD items should have the following spacing:
//  -------------------------------------------------
// |      12        12      10     12        12      |
// |     ----      ----     |     ----      ----     |
// | 10 |icon| 10 |icon| 10 | 10 |icon| 10 |icon| 10 |
// |     ----      ----     |     ----      ----     |
// |      12        12      10     12        12      |
//  -------------------------------------------------
//  where the 12 point vertical spacing is anchored to the smallest icon image,
//  and all subsequent icons with be centered vertically with the smallest icon.

static const CGFloat layerVerticalOffset = 40;
static const CGFloat layerCornerRadius = 12;
static const CGFloat layerGrayComponent = 0;
static const CGFloat layerAlpha = 0.75;
static const CGFloat layerImageScale = 1.5;
static const CGFloat layerSeperatorControllerSize = 1.5;
static const CGFloat layerControllerHorizontalMargin = 10.0;
static const CGFloat layerImageVerticalMargin = 12.0;
static const CGFloat layerSeperatorVerticalMargin = 10.0;
static const CGFloat controlLayerNormalAlpha = 0.75;
static const CGFloat controlLayerDownAlpha = 0.45;

static NSString * const PDFHUDZoomInControl = @"plus.magnifyingglass";
static NSString * const PDFHUDZoomOutControl = @"minus.magnifyingglass";
static NSString * const PDFHUDLaunchPreviewControl = @"preview";
static NSString * const PDFHUDSavePDFControl = @"arrow.down.circle";
static NSString * const PDFHUDSeperatorControl = @"PDFHUDSeperatorControl";

static const CGFloat layerFadeInTimeInterval = 0.25;
static const CGFloat layerFadeOutTimeInterval = 0.5;
static const CGFloat initialHideTimeInterval = 3.0;

static NSArray<NSString *> *controlArray()
{
    return @[
        PDFHUDZoomOutControl,
        PDFHUDZoomInControl,
        PDFHUDSeperatorControl,
        PDFHUDLaunchPreviewControl,
        PDFHUDSavePDFControl
    ];
}

@implementation WKPDFHUDView {
@private
    WeakPtr<WebKit::WebPageProxy> _page;
    RetainPtr<NSString> _activeControl;
    WebKit::PDFPluginIdentifier _pluginIdentifier;
    CGFloat _deviceScaleFactor;
    RetainPtr<CALayer> _layer;
    RetainPtr<CALayer> _activeLayer;
    CGSize _frameSize;
    RetainPtr<NSMutableDictionary<NSString *, NSImage *>> _cachedIcons;
    BOOL _visible;
    BOOL _mouseMovedToHUD;
    BOOL _initialHideTimerFired;
}

- (instancetype)initWithFrame:(NSRect)frame pluginIdentifier:(WebKit::PDFPluginIdentifier)pluginIdentifier page:(WebKit::WebPageProxy&)page
{
    if (!(self = [super initWithFrame:frame]))
        return nil;
    
    self.wantsLayer = YES;
    _cachedIcons = adoptNS([[NSMutableDictionary alloc] init]);
    _pluginIdentifier = pluginIdentifier;
    _page = makeWeakPtr(page);
    _deviceScaleFactor = page.deviceScaleFactor();
    _visible = YES;
    [self _setupLayer:self.layer];
    [self setFrame:frame];

    WeakObjCPtr<WKPDFHUDView> weakSelf = self;
    dispatch_after(dispatch_time(DISPATCH_TIME_NOW, (int64_t)(initialHideTimeInterval * NSEC_PER_SEC)), dispatch_get_main_queue(), ^{
        [weakSelf _hideTimerFired];
    });
    return self;
}

- (void)dealloc
{
    [_layer removeFromSuperlayer];
    [super dealloc];
}

- (void)setFrame:(NSRect)rect
{
    [super setFrame:rect];
    _frameSize = rect.size;

    [CATransaction begin];
    [CATransaction setDisableActions:YES];
    CGRect layerBounds = [_layer bounds];
    [_layer setFrame:CGRectMake(rect.size.width / 2.0 - layerBounds.size.width / 2.0, layerVerticalOffset, layerBounds.size.width, layerBounds.size.height)];
    [CATransaction commit];
}

- (void)setDeviceScaleFactor:(CGFloat)deviceScaleFactor
{
    if (_deviceScaleFactor == deviceScaleFactor)
        return;
    
    _deviceScaleFactor = deviceScaleFactor;
    
    [self _redrawLayer];
}

- (void)_hideTimerFired
{
    _initialHideTimerFired = YES;
    if (!_mouseMovedToHUD)
        [self _setVisible:false];
}

- (void)_setVisible:(bool)isVisible
{
    if (_visible == isVisible)
        return;
    _visible = isVisible;
    [CATransaction begin];
    [CATransaction setAnimationDuration:isVisible ? layerFadeInTimeInterval : layerFadeOutTimeInterval];
    [self _setLayerOpacity:isVisible ? layerAlpha : 0.0];
    [CATransaction commit];
}

- (void)mouseMoved:(NSEvent *)event
{
    if (CGRectContainsPoint([self convertRect:CGRectInset([_layer frame], -16.0, -16.0) toView:nil], NSPointToCGPoint(event.locationInWindow))) {
        [self _setVisible:true];
        _mouseMovedToHUD = YES;
    } else if (_initialHideTimerFired)
        [self _setVisible:false];
}

- (void)mouseDown:(NSEvent *)event
{
    _activeControl = [self _controlForEvent:event];
    if ([_activeControl isEqualToString:PDFHUDSeperatorControl])
        _activeControl = nil;
    if (_activeControl) {
        // Update rendering to highlight it..
        _activeLayer = [self _layerForEvent:event];
        
        // Update layer image; do not animate
        [CATransaction begin];
        [CATransaction setDisableActions:YES];
        
        [_activeLayer setOpacity:controlLayerDownAlpha];
        
        [CATransaction commit];
    }
}

- (void)mouseUp:(NSEvent *)event
{
    if (!_activeControl)
        return;
    
    NSString* mouseUpControl = [self _controlForEvent:event];
    if ([_activeControl isEqualToString:mouseUpControl])
        [self _performActionForControl:_activeControl.get()];
    
    [CATransaction begin];
    [CATransaction setDisableActions:YES];
    [_activeLayer setOpacity:controlLayerNormalAlpha];
    [CATransaction commit];

    _activeLayer = nil;
    _activeControl = nil;
}

- (Optional<NSUInteger>)_controlIndexForEvent:(NSEvent *)event
{
    CGPoint initialPoint = NSPointToCGPoint(event.locationInWindow);
    initialPoint.x -= [_layer frame].origin.x;
    initialPoint.y -= [_layer frame].origin.y;
    for (NSUInteger index = 0; index < [_layer sublayers].count; index++) {
        CALayer *subLayer = [_layer sublayers][index];
        NSRect windowSpaceRect = [self convertRect:subLayer.frame toView:nil];
        if (CGRectContainsPoint(windowSpaceRect, initialPoint))
            return index;
    }
    return WTF::nullopt;
}

- (NSString *)_controlForEvent:(NSEvent *)event
{
    if (auto index = [self _controlIndexForEvent:event])
        return controlArray()[*index];
    return nil;
}

- (CALayer *)_layerForEvent:(NSEvent *)event
{
    if (auto index = [self _controlIndexForEvent:event])
        return [_layer sublayers][*index];
    return nil;
}

- (void)_performActionForControl:(NSString *)control
{
    if (!_visible)
        return;
    auto* page = _page.get();
    if (!page)
        return;
    if ([control isEqualToString:PDFHUDZoomInControl])
        page->pdfZoomIn(_pluginIdentifier);
    else if ([control isEqualToString:PDFHUDZoomOutControl])
        page->pdfZoomOut(_pluginIdentifier);
    else if ([control isEqualToString:PDFHUDSavePDFControl])
        page->pdfSaveToPDF(_pluginIdentifier);
    else if ([control isEqualToString:PDFHUDLaunchPreviewControl])
        page->pdfOpenWithPreview(_pluginIdentifier);
}

- (void)_loadIconImages
{
    for (NSString *controlName in controlArray())
        [self _getImageForControlName:controlName];
}

- (void)_setupLayer:(CALayer *)parentLayer
{
    _layer = adoptNS([[CALayer alloc] init]);
    [_layer setCornerRadius:layerCornerRadius];
    
    [_layer setBackgroundColor:WebCore::cachedCGColor({ WebCore::SRGBA<float>(layerGrayComponent, layerGrayComponent, layerGrayComponent) })];
    [self _setLayerOpacity:layerAlpha];
    
    [self _loadIconImages];
    CGFloat minIconImageHeight = std::numeric_limits<CGFloat>::max();
    for (NSImage *image in [_cachedIcons allValues])
        minIconImageHeight = std::min(minIconImageHeight, (image.size.height / _deviceScaleFactor));
    
    CGFloat dx = layerControllerHorizontalMargin;
    for (NSString *controlName in controlArray()) {
        auto controlLayer = adoptNS([[CALayer alloc] init]);
        CGFloat dy = 0.0;
        CGFloat controllerWidth = 0.0;
        CGFloat controllerHeight = 0.0;

        if ([controlName isEqualToString:PDFHUDSeperatorControl]) {
            dy = layerSeperatorVerticalMargin;
            controllerWidth = layerSeperatorControllerSize;
            controllerHeight = minIconImageHeight + (2.0 * layerImageVerticalMargin) - (2.0 * layerSeperatorVerticalMargin);
            
            [controlLayer setBackgroundColor:[[NSColor lightGrayColor] CGColor]];
        } else {
            NSImage *controlImage = [self _getImageForControlName:controlName];
            [controlLayer setContents:controlImage];
            [controlLayer setOpacity:controlLayerNormalAlpha];

            dy = layerImageVerticalMargin;
            controllerWidth = [controlImage size].width / _deviceScaleFactor;
            controllerHeight = [controlImage size].height / _deviceScaleFactor;
            
            dy -= (controllerHeight - minIconImageHeight) / 2.0;
            
            [controlLayer setFilters:@[[CAFilter filterWithType:kCAFilterColorInvert]]];
        }
        
        [controlLayer setFrame:CGRectMake(dx, dy, controllerWidth, controllerHeight)];
        [_layer addSublayer:controlLayer.get()];
        
        dx += controllerWidth + layerControllerHorizontalMargin;
    }
    
    [_layer setFrame:CGRectMake(0, layerVerticalOffset, dx, minIconImageHeight + 2.0 * layerImageVerticalMargin)];
    [parentLayer addSublayer:_layer.get()];
}

- (void)_redrawLayer
{
    [_cachedIcons removeAllObjects];
    CALayer *parentLayer = [_layer superlayer];
    [_layer removeFromSuperlayer];
    [self _setupLayer:parentLayer];
    [self setFrameSize:_frameSize];
}

- (NSImage *)_getImageForControlName:(NSString *)control
{
    NSImage *iconImage = _cachedIcons.get()[control];
    if (iconImage)
        return iconImage;

    iconImage = [NSImage _imageWithSystemSymbolName:control];
    if (!iconImage)
        return nil;

    // This was introduced in rdar://problem/59118412
    iconImage = [iconImage _imageWithConfiguration:@{
        NSImageAlternateCriterionFont:[NSFont preferredFontForTextStyle:NSFontTextStyleTitle2 options:@{ }],
        NSImageAlternateCriterionSymbolScale:@(NSImageSymbolScaleLarge)
    }];
        
    NSRect iconImageRect = NSMakeRect(0, 0, [iconImage size].width * layerImageScale * _deviceScaleFactor, [iconImage size].height * layerImageScale * _deviceScaleFactor);
    NSImageRep *iconImageRep = [iconImage bestRepresentationForRect:iconImageRect context:nil hints:nil];
    iconImage = [NSImage imageWithImageRep:iconImageRep];
    
    _cachedIcons.get()[control] = iconImage;
    return iconImage;
}

- (void)_setLayerOpacity:(CGFloat)alpha
{
    [_layer setOpacity:alpha];
    for (CALayer *subLayer in [_layer sublayers])
        [subLayer setOpacity:alpha];
}

@end

#endif // ENABLE(UI_PROCESS_PDF_HUD)
