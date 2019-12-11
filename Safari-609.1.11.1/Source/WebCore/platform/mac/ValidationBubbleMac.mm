/*
 * Copyright (C) 2016 Apple Inc. All rights reserved.
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

#if PLATFORM(MAC)
#import "ValidationBubble.h"

#import <AppKit/AppKit.h>
#import <wtf/text/WTFString.h>

@interface WebValidationPopover : NSPopover
@end

@implementation WebValidationPopover

- (void)mouseDown:(NSEvent *)event
{
    UNUSED_PARAM(event);
    [self close];
}

@end

namespace WebCore {

static const CGFloat horizontalPadding = 5;
static const CGFloat verticalPadding = 5;
static const CGFloat maxLabelWidth = 300;

ValidationBubble::ValidationBubble(NSView* view, const String& message, const Settings& settings)
    : m_view(view)
    , m_message(message)
{
    RetainPtr<NSViewController> controller = adoptNS([[NSViewController alloc] init]);

    RetainPtr<NSView> popoverView = adoptNS([[NSView alloc] initWithFrame:NSZeroRect]);
    [controller setView:popoverView.get()];

    RetainPtr<NSTextField> label = adoptNS([[NSTextField alloc] init]);
    [label setEditable:NO];
    [label setDrawsBackground:NO];
    [label setBordered:NO];
    [label setStringValue:message];
    m_fontSize = std::max(settings.minimumFontSize, 13.0);
    [label setFont:[NSFont systemFontOfSize:m_fontSize]];
    [label setMaximumNumberOfLines:4];
    [[label cell] setTruncatesLastVisibleLine:YES];
    [popoverView addSubview:label.get()];
    NSSize labelSize = [label sizeThatFits:NSMakeSize(maxLabelWidth, CGFLOAT_MAX)];
    [label setFrame:NSMakeRect(horizontalPadding, verticalPadding, labelSize.width, labelSize.height)];
    [popoverView setFrame:NSMakeRect(0, 0, labelSize.width + horizontalPadding * 2, labelSize.height + verticalPadding * 2)];

    m_popover = adoptNS([[WebValidationPopover alloc] init]);
    [m_popover setContentViewController:controller.get()];
    [m_popover setBehavior:NSPopoverBehaviorTransient];
    [m_popover setAnimates:NO];
}

ValidationBubble::~ValidationBubble()
{
    [m_popover close];
}

void ValidationBubble::showRelativeTo(const IntRect& anchorRect)
{
    NSRect rect = NSMakeRect(anchorRect.x(), anchorRect.y(), anchorRect.width(), anchorRect.height());
    [m_popover showRelativeToRect:rect ofView:m_view preferredEdge:NSMinYEdge];
}

} // namespace WebCore

#endif // PLATFORM(MAC)
