/*
 * Copyright (C) 2012 Apple Inc. All rights reserved.
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
#import "WebCoreFullScreenWarningView.h"

#if PLATFORM(MAC) && ENABLE(FULLSCREEN_API)

#import "LocalizedStrings.h"
#import <wtf/text/WTFString.h>

static const CGFloat WarningViewTextWhite = 0.9;
static const CGFloat WarningViewTextAlpha = 1;
static const CGFloat WarningViewTextSize = 48;
static const CGFloat WarningViewPadding = 20;
static const CGFloat WarningViewCornerRadius = 10;
static const CGFloat WarningViewBorderWhite = 0.9;
static const CGFloat WarningViewBorderAlpha = 0.2;
static const CGFloat WarningViewBackgroundWhite = 0.1;
static const CGFloat WarningViewBackgroundAlpha = 0.9;
static const CGFloat WarningViewShadowWhite = 0.1;
static const CGFloat WarningViewShadowAlpha = 1;
static const NSSize WarningViewShadowOffset = {0, -2};
static const CGFloat WarningViewShadowRadius = 5;

@implementation WebCoreFullScreenWarningView

- (id)initWithTitle:(NSString*)title
{
    self = [super initWithFrame:NSZeroRect];
    if (!self)
        return nil;

    [self setAutoresizingMask:(NSViewMinXMargin | NSViewMaxXMargin | NSViewMinYMargin | NSViewMaxYMargin)];
    [self setBoxType:NSBoxCustom];
    [self setTitlePosition:NSNoTitle];

    _textField = adoptNS([[NSTextField alloc] initWithFrame:NSZeroRect]);
    [_textField setEditable:NO];
    [_textField setSelectable:NO];
    [_textField setBordered:NO];
    [_textField setDrawsBackground:NO];

    NSFont* textFont = [NSFont boldSystemFontOfSize:WarningViewTextSize];
    NSColor* textColor = [NSColor colorWithCalibratedWhite:WarningViewTextWhite alpha:WarningViewTextAlpha];
    RetainPtr<NSDictionary> attributes = adoptNS([[NSDictionary alloc] initWithObjectsAndKeys:
                                                  textFont, NSFontAttributeName,
                                                  textColor, NSForegroundColorAttributeName,
                                                  nil]);
    RetainPtr<NSAttributedString> text = adoptNS([[NSAttributedString alloc] initWithString:title attributes:attributes.get()]);
    [_textField setAttributedStringValue:text.get()];
    [_textField sizeToFit];
    NSRect textFieldFrame = [_textField frame];
    NSSize frameSize = textFieldFrame.size;
    frameSize.width += WarningViewPadding * 2;
    frameSize.height += WarningViewPadding * 2;
    [self setFrameSize:frameSize];

    textFieldFrame.origin = NSMakePoint(
        (frameSize.width - textFieldFrame.size.width) / 2,
        (frameSize.height - textFieldFrame.size.height) / 2);

    // Offset the origin by the font's descender, to center the text field about the baseline:
    textFieldFrame.origin.y += [[_textField font] descender];

    [_textField setFrame:NSIntegralRect(textFieldFrame)];
    [self addSubview:_textField.get()];

    NSColor* backgroundColor = [NSColor colorWithCalibratedWhite:WarningViewBackgroundWhite alpha:WarningViewBackgroundAlpha];
    [self setFillColor:backgroundColor];
    [self setCornerRadius:WarningViewCornerRadius];

    NSColor* borderColor = [NSColor colorWithCalibratedWhite:WarningViewBorderWhite alpha:WarningViewBorderAlpha];
    [self setBorderColor:borderColor];

    RetainPtr<NSShadow> shadow = adoptNS([[NSShadow alloc] init]);
    RetainPtr<NSColor> shadowColor = [NSColor colorWithCalibratedWhite:WarningViewShadowWhite alpha:WarningViewShadowAlpha];
    [shadow setShadowColor:shadowColor.get()];
    [shadow setShadowOffset:WarningViewShadowOffset];
    [shadow setShadowBlurRadius:WarningViewShadowRadius];
    [self setShadow:shadow.get()];

    return self;
}
@end

#endif // PLATFORM(MAC) && ENABLE(FULLSCREEN_API)
