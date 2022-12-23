/*
 * Copyright (C) 2022 Apple Inc. All Rights Reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#import "config.h"
#import "WebControlView.h"

#if PLATFORM(MAC)

#import <pal/spi/mac/CoreUISPI.h>
#import <pal/spi/mac/NSAppearanceSPI.h>
#import <pal/spi/mac/NSCellSPI.h>
#import <pal/spi/mac/NSImageSPI.h>
#import <pal/spi/mac/NSServicesRolloverButtonCellSPI.h>
#import <pal/spi/mac/NSSharingServicePickerSPI.h>
#import <pal/spi/mac/NSTextFieldCellSPI.h>

@implementation WebControlWindow

static BOOL _hasKeyAppearance;

+ (BOOL)hasKeyAppearance
{
    return _hasKeyAppearance;
}

+ (void)setHasKeyAppearance:(BOOL)newHasKeyAppearance
{
    _hasKeyAppearance = newHasKeyAppearance;
}

- (BOOL)hasKeyAppearance
{
    return _hasKeyAppearance;
}

- (BOOL)isKeyWindow
{
    return _hasKeyAppearance;
}

- (BOOL)_needsToResetDragMargins
{
    return false;
}

- (void)_setNeedsToResetDragMargins:(BOOL)needs
{
}
@end

@implementation WebControlView {
    RetainPtr<WebControlWindow> _window;
}

static NSRect _clipBounds;

+ (NSRect)clipBounds
{
    return _clipBounds;
}

+ (void)setClipBounds:(NSRect)newClipBounds
{
    _clipBounds = newClipBounds;
}

- (instancetype)init
{
    if (!(self = [super init]))
        return nil;

    // Using defer:YES prevents us from wasting any window server resources for this window, since we're not actually
    // going to draw into it. The other arguments match what you get when calling -[NSWindow init].
    _window = adoptNS([[WebControlWindow alloc] initWithContentRect:NSMakeRect(100, 100, 100, 100) styleMask:NSWindowStyleMaskBorderless backing:NSBackingStoreBuffered defer:YES]);

    return self;
}

- (NSWindow *)window
{
    return _window.get();
}

- (BOOL)isFlipped
{
    return YES;
}

- (NSText *)currentEditor
{
    return nil;
}

- (BOOL)_automaticFocusRingDisabled
{
    return YES;
}

- (NSRect)_focusRingVisibleRect
{
    if (NSIsEmptyRect(_clipBounds))
        return [self visibleRect];
    return _clipBounds;
}

- (NSView *)_focusRingClipAncestor
{
    return self;
}

- (NSWindow *)_viewRoot
{
    return _window.get();
}

- (void)addSubview:(NSView *)subview
{
    // By doing nothing in this method we forbid controls from adding subviews.
    // This tells AppKit to not use layer-backed animation for control rendering.
    UNUSED_PARAM(subview);
}

@end

@implementation WebControlTextFieldCell

- (CFDictionaryRef)_adjustedCoreUIDrawOptionsForDrawingBordersOnly:(CFDictionaryRef)defaultOptions
{
#if HAVE(OS_DARK_MODE_SUPPORT)
    // Dark mode controls don't have borders, just a semi-transparent background of shadows.
    // In the dark mode case we can't disable borders, or we will not paint anything for the control.
    NSAppearanceName appearance = [self.controlView.effectiveAppearance bestMatchFromAppearancesWithNames:@[ NSAppearanceNameAqua, NSAppearanceNameDarkAqua ]];
    if ([appearance isEqualToString:NSAppearanceNameDarkAqua])
        return defaultOptions;
#endif

    // FIXME: This is a workaround for <rdar://problem/11385461>. When that bug is resolved, we should remove this code,
    // as well as the internal method overrides below.
    auto coreUIDrawOptions = adoptCF(CFDictionaryCreateMutableCopy(NULL, 0, defaultOptions));
    CFDictionarySetValue(coreUIDrawOptions.get(), CFSTR("borders only"), kCFBooleanTrue);
    return coreUIDrawOptions.autorelease();
}

- (CFDictionaryRef)_coreUIDrawOptionsWithFrame:(NSRect)cellFrame inView:(NSView *)controlView includeFocus:(BOOL)includeFocus
{
    return [self _adjustedCoreUIDrawOptionsForDrawingBordersOnly:[super _coreUIDrawOptionsWithFrame:cellFrame inView:controlView includeFocus:includeFocus]];
}

- (CFDictionaryRef)_coreUIDrawOptionsWithFrame:(NSRect)cellFrame inView:(NSView *)controlView includeFocus:(BOOL)includeFocus maskOnly:(BOOL)maskOnly
{
    return [self _adjustedCoreUIDrawOptionsForDrawingBordersOnly:[super _coreUIDrawOptionsWithFrame:cellFrame inView:controlView includeFocus:includeFocus maskOnly:maskOnly]];
}

@end

#endif // PLATFORM(MAC)
