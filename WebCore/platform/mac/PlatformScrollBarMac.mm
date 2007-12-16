/*
 * Copyright (C) 2004, 2006 Apple Computer, Inc.  All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#import "config.h"
#import "PlatformScrollBar.h"

#import "BlockExceptions.h"

using namespace WebCore;

@interface WebCoreScrollBar : NSScroller
{
    PlatformScrollbar* scrollbar;
}

- (id)initWithPlatformScrollbar:(PlatformScrollbar*)s;
- (void)detachPlatformScrollbar;

@end

@implementation WebCoreScrollBar

static NSControlSize NSControlSizeForScrollBarControlSize(ScrollbarControlSize size)
{
    if (size == SmallScrollbar)
        return NSSmallControlSize;
    if (size == MiniScrollbar)
        return NSMiniControlSize;
    return NSRegularControlSize;
}

- (id)initWithPlatformScrollbar:(PlatformScrollbar*)s
{
    // Cocoa scrollbars just set their orientation by examining their own
    // dimensions, so we have to do this unsavory hack.
    NSRect orientation;
    NSControlSize controlSize = NSControlSizeForScrollBarControlSize(s->controlSize());
    orientation.origin.x = orientation.origin.y = 0;
    if (s->orientation() == VerticalScrollbar) {
        orientation.size.width = [NSScroller scrollerWidthForControlSize:controlSize];
        orientation.size.height = 100;
    } else {
        orientation.size.width = 100;
        orientation.size.height = [NSScroller scrollerWidthForControlSize:controlSize];
    }
    self = [self initWithFrame:orientation];

    scrollbar = s;

    [self setEnabled:YES];
    [self setTarget:self];
    [self setAction:@selector(scroll:)];
    [self setControlSize:controlSize];

    return self;
}

- (void)detachPlatformScrollbar
{
    [self setTarget:nil];
    scrollbar = 0;
}

- (IBAction)scroll:(NSScroller*)sender
{
    if (scrollbar)
        scrollbar->scrollbarHit([sender hitPart]);
}

- (void)mouseDown:(NSEvent *)event
{
    Widget::beforeMouseDown(self, scrollbar);
    [super mouseDown:event];
    Widget::afterMouseDown(self, scrollbar);
}

@end

namespace WebCore
{

PlatformScrollbar::PlatformScrollbar(ScrollbarClient* client, ScrollbarOrientation orientation, ScrollbarControlSize controlSize)
    : Scrollbar(client, orientation, controlSize)
{
    BEGIN_BLOCK_OBJC_EXCEPTIONS;

    WebCoreScrollBar *bar = [[WebCoreScrollBar alloc] initWithPlatformScrollbar:this];
    setView(bar);
    [bar release];

    END_BLOCK_OBJC_EXCEPTIONS;
}

PlatformScrollbar::~PlatformScrollbar()
{
    WebCoreScrollBar* bar = (WebCoreScrollBar*)getView();
    [bar detachPlatformScrollbar];

    // Widget should probably do this for all widgets.
    // But we don't need it for form elements, and for frames it doesn't work
    // well because of the way the NSViews are created in WebKit. So for now,
    // we'll just do it explictly for Scrollbar.
    removeFromSuperview();
}

void PlatformScrollbar::updateThumbPosition()
{
    BEGIN_BLOCK_OBJC_EXCEPTIONS;
    WebCoreScrollBar *bar = (WebCoreScrollBar *)getView();
    [bar setFloatValue:(float)m_currentPos / (m_totalSize - m_visibleSize)
        knobProportion:[bar knobProportion]];
    END_BLOCK_OBJC_EXCEPTIONS;
}

void PlatformScrollbar::updateThumbProportion()
{
    float val = static_cast<float>(m_visibleSize) / m_totalSize;

    BEGIN_BLOCK_OBJC_EXCEPTIONS;
    WebCoreScrollBar *bar = (WebCoreScrollBar *)getView();
    if (val != [bar knobProportion] && val >= 0)
        [bar setFloatValue:static_cast<float>(m_currentPos) / (m_totalSize - m_visibleSize) knobProportion:val];
    END_BLOCK_OBJC_EXCEPTIONS;
}

bool PlatformScrollbar::scrollbarHit(NSScrollerPart hitPart)
{
    int maxPos = m_totalSize - m_visibleSize;
    if (maxPos <= 0)
        return false; // Impossible to scroll anywhere.
    
    WebCoreScrollBar *bar = (WebCoreScrollBar *)getView();
    int newPos = value();
    switch (hitPart) {
        case NSScrollerDecrementLine:
            newPos -= m_lineStep;
            break;
        case NSScrollerIncrementLine:
            newPos += m_lineStep;
            break;
        case NSScrollerDecrementPage:
            newPos -= m_pageStep;
            break;
        case NSScrollerIncrementPage:
            newPos += m_pageStep;
            break;

            // If the thumb is hit, then the scrollbar changed its value for us.
        case NSScrollerKnob:
        case NSScrollerKnobSlot:
            newPos = (int)([bar floatValue] * maxPos);
            break;

        case NSScrollerNoPart:
            break;
    }

    return setValue(newPos);
}

int PlatformScrollbar::width() const
{
    return Widget::width();
}

int PlatformScrollbar::height() const
{
    return Widget::height();
}

void PlatformScrollbar::setRect(const IntRect& rect)
{
    setFrameGeometry(rect);
}

void PlatformScrollbar::setEnabled(bool enabled)
{
    Widget::setEnabled(enabled);
}

void PlatformScrollbar::paint(GraphicsContext* graphicsContext, const IntRect& damageRect)
{
    Widget::paint(graphicsContext, damageRect);
}

}
