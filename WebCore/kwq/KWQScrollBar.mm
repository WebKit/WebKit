/*
 * Copyright (C) 2003 Apple Computer, Inc.  All rights reserved.
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

#import "KWQScrollBar.h"

#import "KWQExceptions.h"

@interface KWQScrollBar : NSScroller
{
    QScrollBar* scrollBar;
}

- (id)initWithQScrollBar:(QScrollBar*)s;

@end

@implementation KWQScrollBar


- (id)initWithQScrollBar:(QScrollBar*)s
{
    scrollBar = s;

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
    id result = [self initWithFrame: orientation];
    [result setEnabled: YES];
    [self setTarget:self];
    [self setAction:@selector(scroll:)];

    return result;
}

-(IBAction)scroll:(NSScroller*)sender
{
    scrollBar->scrollbarHit([sender hitPart]);
}
@end

QScrollBar::QScrollBar(Qt::Orientation orientation, QWidget* parent)
:m_valueChanged(this, SIGNAL(valueChanged(int)))
{
    m_orientation = orientation;
    m_visibleSize = 0;
    m_totalSize = 0;
    m_currentPos = 0;
    m_lineStep = 0;
    m_pageStep = 0;
    m_scroller = 0;

    KWQ_BLOCK_EXCEPTIONS;
    m_scroller = [[KWQScrollBar alloc] initWithQScrollBar:this];
    setView(m_scroller);
    [m_scroller release];
    [parent->getView() addSubview: m_scroller];
    KWQ_UNBLOCK_EXCEPTIONS;

    setFocusPolicy(NoFocus);
}

QScrollBar::~QScrollBar()
{
    KWQ_BLOCK_EXCEPTIONS;
    [m_scroller removeFromSuperview];
    KWQ_UNBLOCK_EXCEPTIONS;
}

void QScrollBar::setValue(int v)
{
    int maxPos = m_totalSize - m_visibleSize;
    if (v < 0) v = 0;
    if (v > maxPos)
        v = maxPos;
    if (m_currentPos == v)
        return; // Our value stayed the same.
    m_currentPos = v;
    KWQ_BLOCK_EXCEPTIONS;
    [m_scroller setFloatValue: (float)m_currentPos/maxPos
               knobProportion: [m_scroller knobProportion]];
    KWQ_UNBLOCK_EXCEPTIONS;
    valueChanged(); // Emit the signal that indicates our value has changed.
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
    if (!(val == [m_scroller knobProportion] || val < 0.0))
	[m_scroller setFloatValue: [m_scroller floatValue] knobProportion: val];
    KWQ_UNBLOCK_EXCEPTIONS;
}

void QScrollBar::scrollbarHit(NSScrollerPart hitPart)
{
    int maxPos = m_totalSize - m_visibleSize;
    if (maxPos <= 0)
        return; // Impossible to scroll anywhere.
    
    volatile int newPos = m_currentPos;
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
	    KWQ_BLOCK_EXCEPTIONS;
            newPos = (int)([m_scroller floatValue]*maxPos);
	    KWQ_UNBLOCK_EXCEPTIONS;
            break;
        default: ;
    }

    setValue(newPos);
}

void QScrollBar::valueChanged()
{
    m_valueChanged.call(m_currentPos);
}
