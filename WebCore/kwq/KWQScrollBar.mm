/*
 * Copyright (C) 2004 Apple Computer, Inc.  All rights reserved.
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

#include "config.h"
#import "KWQScrollBar.h"

#import "KWQExceptions.h"
#import "KWQView.h"

@interface KWQScrollBar : NSScroller <KWQWidgetHolder>
{
    QScrollBar* scrollBar;
}

- (id)initWithQScrollBar:(QScrollBar *)s;
- (void)detachQScrollBar;

@end

@implementation KWQScrollBar

- (id)initWithQScrollBar:(QScrollBar *)s
{
    // Cocoa scrollbars just set their orientation by examining their own
    // dimensions, so we have to do this unsavory hack.
    NSRect orientation;
    orientation.origin.x = orientation.origin.y = 0;
    if ( s->orientation() == Qt::Vertical ) {
        orientation.size.width = [NSScroller scrollerWidth];
        orientation.size.height = 100;
    }
    else {
        orientation.size.width = 100;
        orientation.size.height = [NSScroller scrollerWidth];
    }
    self = [self initWithFrame:orientation];

    scrollBar = s;

    [self setEnabled:YES];
    [self setTarget:self];
    [self setAction:@selector(scroll:)];

    return self;
}

- (void)detachQScrollBar
{
    [self setTarget:nil];
    scrollBar = 0;
}

- (IBAction)scroll:(NSScroller*)sender
{
    if (scrollBar) {
        scrollBar->scrollbarHit([sender hitPart]);
    }
}

- (QWidget *)widget
{
    return scrollBar;
}

- (void)mouseDown:(NSEvent *)event
{
    QWidget::beforeMouseDown(self);
    [super mouseDown:event];
    QWidget::afterMouseDown(self);
}

@end

QScrollBar::QScrollBar(Orientation orientation, QWidget* parent)
    : m_orientation(orientation)
    , m_visibleSize(0)
    , m_totalSize(0)
    , m_currentPos(0)
    , m_lineStep(0)
    , m_pageStep(0)
    , m_valueChanged(this, SIGNAL(valueChanged(int)))
{
    KWQ_BLOCK_EXCEPTIONS;

    KWQScrollBar *bar = [[KWQScrollBar alloc] initWithQScrollBar:this];
    setView(bar);
    [bar release];

    KWQ_UNBLOCK_EXCEPTIONS;
}

QScrollBar::~QScrollBar()
{
    KWQScrollBar *bar = (KWQScrollBar *)getView();
    [bar detachQScrollBar];

    // QWidget should probably do this for all widgets.
    // But we don't need it for form elements, and for frames it doesn't work
    // well because of the way the NSViews are created in WebKit. So for now,
    // we'll just do it explictly for QScrollBar.
    [bar removeFromSuperview];
}

bool QScrollBar::setValue(int v)
{
    int maxPos = m_totalSize - m_visibleSize;
    if (v < 0) v = 0;
    if (v > maxPos)
        v = maxPos;
    if (m_currentPos == v)
        return false; // Our value stayed the same.
    m_currentPos = v;

    KWQ_BLOCK_EXCEPTIONS;
    KWQScrollBar *bar = (KWQScrollBar *)getView();
    [bar setFloatValue:(float)m_currentPos/maxPos
        knobProportion:[bar knobProportion]];
    KWQ_UNBLOCK_EXCEPTIONS;

    valueChanged();
    
    return true;
}

void QScrollBar::setSteps(int lineStep, int pageStep)
{
    m_lineStep = lineStep;
    m_pageStep = pageStep;
}

void QScrollBar::setKnobProportion(int visibleArea, int totalArea)
{
    m_visibleSize = visibleArea;
    m_totalSize = totalArea;
    float val = (float)m_visibleSize/m_totalSize;

    KWQ_BLOCK_EXCEPTIONS;
    KWQScrollBar *bar = (KWQScrollBar *)getView();
    if (!(val == [bar knobProportion] || val < 0.0))
        [bar setFloatValue: [bar floatValue] knobProportion: val];
    KWQ_UNBLOCK_EXCEPTIONS;
}

bool QScrollBar::scrollbarHit(NSScrollerPart hitPart)
{
    int maxPos = m_totalSize - m_visibleSize;
    if (maxPos <= 0)
        return false; // Impossible to scroll anywhere.
    
    KWQScrollBar *bar = (KWQScrollBar *)getView();
    int newPos = m_currentPos;
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

void QScrollBar::valueChanged()
{
    m_valueChanged.call(m_currentPos);
}

bool QScrollBar::scroll(KWQScrollDirection direction, KWQScrollGranularity granularity, float multiplier)
{
    float delta = 0.0;
    if ((direction == KWQScrollUp && m_orientation == Vertical) || (direction == KWQScrollLeft && m_orientation == Horizontal)) {
        if (granularity == KWQScrollLine) {
            delta = -m_lineStep;
        } else if (granularity == KWQScrollPage) {
            delta = -m_pageStep;
        } else if (granularity == KWQScrollDocument) {
            delta = -m_currentPos;
        } else if (granularity == KWQScrollWheel) {
            delta = -m_lineStep;
        }
    } else if ((direction == KWQScrollDown && m_orientation == Vertical) || (direction == KWQScrollRight && m_orientation == Horizontal)) {
        if (granularity == KWQScrollLine) {
            delta = m_lineStep;
        } else if (granularity == KWQScrollPage) {
            delta = m_pageStep;
        } else if (granularity == KWQScrollDocument) {
            delta = m_totalSize - m_visibleSize - m_currentPos;
        } else if (granularity == KWQScrollWheel) {
            delta = m_lineStep;
        }
    }
    int newPos = (int)(m_currentPos + (delta * multiplier));
    return setValue(newPos);
}

