/*
 * Copyright (C) 2018 Apple Inc. All rights reserved.
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
#import "TestFontOptions.h"

#if PLATFORM(MAC) && WK_API_ENABLED

#import "AppKitSPI.h"
#import "ClassMethodSwizzler.h"
#import <objc/runtime.h>
#import <wtf/RetainPtr.h>
#import <wtf/SetForScope.h>

static TestFontOptions *sharedFontOptionsForTesting()
{
    return TestFontOptions.sharedInstance;
}

@interface TestFontOptions ()
- (instancetype)initWithFontOptions:(NSFontOptions *)fontOptions;
@end

@implementation TestFontOptions {
    RetainPtr<NSFontOptions> _fontOptions;
    CGSize _shadowOffset;
    CGFloat _shadowBlurRadius;
    BOOL _hasShadow;
    BOOL _hasPendingShadowChanges;

    RetainPtr<NSColor> _foregroundColor;
    RetainPtr<NSColor> _backgroundColor;
    BOOL _hasPendingColorChanges;

    std::unique_ptr<ClassMethodSwizzler> _replaceFontOptionsSwizzler;
    RetainPtr<NSDictionary> _selectedAttributes;
    BOOL _hasMultipleFonts;
}

@synthesize hasShadow=_hasShadow;
@synthesize shadowBlurRadius=_shadowBlurRadius;
@synthesize hasMultipleFonts=_hasMultipleFonts;

+ (instancetype)sharedInstance
{
    static TestFontOptions *sharedInstance;
    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
        NSFontOptions *sharedFontOptions = [NSClassFromString(@"NSFontOptions") sharedFontOptions];
        sharedInstance = [[TestFontOptions alloc] initWithFontOptions:sharedFontOptions];
    });
    return sharedInstance;
}

- (instancetype)initWithFontOptions:(NSFontOptions *)fontOptions
{
    if (!(self = [super init]))
        return nil;

    _fontOptions = fontOptions;
    ASSERT(_fontOptions);

    _shadowOffset = CGSizeZero;
    _shadowBlurRadius = 0;
    _replaceFontOptionsSwizzler = std::make_unique<ClassMethodSwizzler>(NSClassFromString(@"NSFontOptions"), @selector(sharedFontOptions), reinterpret_cast<IMP>(sharedFontOptionsForTesting));
    _hasPendingShadowChanges = NO;
    _hasMultipleFonts = NO;

    return self;
}

- (NSDictionary *)selectedAttributes
{
    return _selectedAttributes.get();
}

- (NSFontOptions *)fontOptions
{
    return _fontOptions.get();
}

- (CGFloat)shadowWidth
{
    return _shadowOffset.width;
}

- (void)setShadowWidth:(CGFloat)shadowWidth
{
    if (_shadowOffset.width == shadowWidth)
        return;

    SetForScope<BOOL> hasPendingFontShadowChanges(_hasPendingShadowChanges, YES);
    _shadowOffset.width = shadowWidth;
    [self _dispatchFontAttributeChanges];
}

- (CGFloat)shadowHeight
{
    return _shadowOffset.height;
}

- (void)setShadowHeight:(CGFloat)shadowHeight
{
    if (_shadowOffset.height == shadowHeight)
        return;

    SetForScope<BOOL> hasPendingFontShadowChanges(_hasPendingShadowChanges, YES);
    _shadowOffset.height = shadowHeight;
    [self _dispatchFontAttributeChanges];
}

- (void)setShadowBlurRadius:(CGFloat)shadowBlurRadius
{
    if (_shadowBlurRadius == shadowBlurRadius)
        return;

    SetForScope<BOOL> hasPendingFontShadowChanges(_hasPendingShadowChanges, YES);
    _shadowBlurRadius = shadowBlurRadius;
    [self _dispatchFontAttributeChanges];
}

- (void)setHasShadow:(BOOL)hasShadow
{
    if (_hasShadow == hasShadow)
        return;

    SetForScope<BOOL> hasPendingFontShadowChanges(_hasPendingShadowChanges, YES);
    _hasShadow = hasShadow;
    [self _dispatchFontAttributeChanges];
}

- (NSColor *)foregroundColor
{
    return _foregroundColor.get();
}

- (void)setForegroundColor:(NSColor *)color
{
    SetForScope<BOOL> hasPendingColorChanges(_hasPendingColorChanges, YES);
    _foregroundColor = adoptNS([color copy]);
    [self _dispatchFontAttributeChanges];
}

- (NSColor *)backgroundColor
{
    return _backgroundColor.get();
}

- (void)setBackgroundColor:(NSColor *)color
{
    SetForScope<BOOL> hasPendingColorChanges(_hasPendingColorChanges, YES);
    _backgroundColor = adoptNS([color copy]);
    [self _dispatchFontAttributeChanges];
}

- (void)_dispatchFontAttributeChanges
{
    [NSFontManager.sharedFontManager.target performSelector:@selector(changeAttributes:) withObject:self];
}

- (NSDictionary *)convertAttributes:(NSDictionary *)attributes
{
    auto convertedAttributes = adoptNS([attributes mutableCopy]);

    if (_hasPendingShadowChanges) {
        if (_hasShadow) {
            auto shadow = adoptNS([[NSShadow alloc] init]);
            [shadow setShadowBlurRadius:_shadowBlurRadius];
            [shadow setShadowOffset:_shadowOffset];
            [convertedAttributes setObject:shadow.get() forKey:NSShadowAttributeName];
        } else
            [convertedAttributes removeObjectForKey:NSShadowAttributeName];
    }

    if (_hasPendingColorChanges) {
        if (_foregroundColor)
            [convertedAttributes setObject:_foregroundColor.get() forKey:NSForegroundColorAttributeName];
        else
            [convertedAttributes removeObjectForKey:NSForegroundColorAttributeName];

        if (_backgroundColor)
            [convertedAttributes setObject:_backgroundColor.get() forKey:NSBackgroundColorAttributeName];
        else
            [convertedAttributes removeObjectForKey:NSBackgroundColorAttributeName];
    }

    return convertedAttributes.autorelease();
}

- (void)setSelectedAttributes:(NSDictionary *)attributes isMultiple:(BOOL)multiple
{
    _hasMultipleFonts = multiple;
    _selectedAttributes = attributes;
}

- (void)forwardInvocation:(NSInvocation *)invocation
{
    if ([_fontOptions respondsToSelector:invocation.selector]) {
        [invocation invokeWithTarget:_fontOptions.get()];
        return;
    }

    [super forwardInvocation:invocation];
}

@end

#endif // PLATFORM(MAC) && WK_API_ENABLED
