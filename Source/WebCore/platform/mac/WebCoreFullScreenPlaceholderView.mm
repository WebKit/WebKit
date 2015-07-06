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

#include "config.h"
#if !PLATFORM(IOS)

#include "WebCoreFullScreenPlaceholderView.h"

#include "LocalizedStrings.h"
#include "WebCoreFullScreenWarningView.h"
#include <wtf/text/WTFString.h>

@interface CAFilter : NSObject
+ (CAFilter *)filterWithType:(NSString *)type;
@end

using namespace WebCore;

@implementation WebCoreFullScreenPlaceholderView
- (id)initWithFrame:(NSRect)frameRect
{
    self = [super initWithFrame:frameRect];
    if (!self)
        return nil;

#if __MAC_OS_X_VERSION_MIN_REQUIRED >= 101000
    self.wantsLayer = YES;
    self.autoresizesSubviews = YES;
    self.layerContentsPlacement = NSViewLayerContentsPlacementScaleProportionallyToFit;
    self.layerContentsRedrawPolicy = NSViewLayerContentsRedrawNever;

    _effectView = adoptNS([[NSVisualEffectView alloc] initWithFrame:frameRect]);
    _effectView.get().wantsLayer = YES;
    _effectView.get().autoresizesSubviews = YES;
    _effectView.get().autoresizingMask = NSViewWidthSizable | NSViewHeightSizable;
    _effectView.get().blendingMode = NSVisualEffectBlendingModeWithinWindow;
    _effectView.get().hidden = YES;
    _effectView.get().material = NSVisualEffectMaterialLight;
    _effectView.get().state = NSVisualEffectStateActive;
    [self addSubview:_effectView.get()];

    _exitWarning = adoptNS([[NSTextField alloc] initWithFrame:NSZeroRect]);
    _exitWarning.get().autoresizingMask = NSViewMinXMargin | NSViewMaxXMargin | NSViewMinYMargin | NSViewMaxYMargin;
    _exitWarning.get().bordered = NO;
    _exitWarning.get().drawsBackground = NO;
    _exitWarning.get().editable = NO;
    _exitWarning.get().font = [NSFont systemFontOfSize:27];
    _exitWarning.get().selectable = NO;
    _exitWarning.get().stringValue = clickToExitFullScreenText();
    _exitWarning.get().textColor = [NSColor tertiaryLabelColor];
    [_exitWarning sizeToFit];

    NSRect warningFrame = [_exitWarning.get() frame];
    warningFrame.origin = NSMakePoint((frameRect.size.width - warningFrame.size.width) / 2, frameRect.size.height / 2);
    _exitWarning.get().frame = warningFrame;
    [_effectView addSubview:_exitWarning.get()];
#else
    [self setLayer:[CALayer layer]];
    [self setLayerContentsRedrawPolicy:NSViewLayerContentsRedrawNever];
    [self setWantsLayer:YES];

    _exitWarning = adoptNS([[WebCoreFullScreenWarningView alloc] initWithTitle:clickToExitFullScreenText()]);
    NSRect warningFrame = [_exitWarning.get() frame];
    warningFrame.origin = NSMakePoint((frameRect.size.width - warningFrame.size.width) / 2, (frameRect.size.height - warningFrame.size.height) / 2);
    [_exitWarning.get() setFrame:warningFrame];
    [_exitWarning.get() setHidden:YES];
    [self addSubview:_exitWarning.get()];
#endif

    return self;
}

@synthesize target = _target;
@synthesize action = _action;

@dynamic contents;
- (void)setContents:(id)contents
{
    [[self layer] setContents:contents];
}

- (id)contents
{
    return [[self layer] contents];
}

- (void)setExitWarningVisible:(BOOL)visible
{
#if __MAC_OS_X_VERSION_MIN_REQUIRED >= 101000
    [_effectView setHidden:!visible];
#else
    [_exitWarning.get() setHidden:!visible];
    if (visible) {
        CAFilter* filter = [CAFilter filterWithType:@"colorMonochrome"];
        [filter setValue:[NSNumber numberWithFloat:-0.2] forKey:@"inputBias"];
        [filter setValue:[NSNumber numberWithFloat:1] forKey:@"inputAmount"];
        [[self layer] setFilters:[NSArray arrayWithObject:filter]];
    } else
        [[self layer] setFilters:nil];
#endif
}

- (void)mouseDown:(NSEvent *)theEvent
{
    UNUSED_PARAM(theEvent);

    if (!_target || !_action)
        return;
    [_target performSelector:_action];
}

@end

#endif // !PLATFORM(IOS)
