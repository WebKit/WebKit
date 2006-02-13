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

#ifndef KWQSCROLLBAR_H_
#define KWQSCROLLBAR_H_

#include "Widget.h"
#include "KWQNamespace.h"

#ifdef __OBJC__
@class NSScroller;
#else
class NSScroller;
typedef int NSScrollerPart;
#endif

typedef enum {
    KWQScrollUp,
    KWQScrollDown,
    KWQScrollLeft,
    KWQScrollRight
} KWQScrollDirection;

typedef enum {
    KWQScrollLine,
    KWQScrollPage,
    KWQScrollDocument,
    KWQScrollWheel
} KWQScrollGranularity;

class QScrollBar : public Widget {
public:
    QScrollBar(Orientation orientation, Widget* parent);
    ~QScrollBar();

    Orientation orientation() { return m_orientation; }

    int value() { return m_currentPos; }
    bool setValue(int v);

    void setSteps(int lineStep, int pageStep);
    void setKnobProportion(int visibleSize, int totalSize);
    
    bool scrollbarHit(NSScrollerPart);
    void valueChanged();
    
    bool scroll(KWQScrollDirection, KWQScrollGranularity, float multiplier = 1.0);
    
private:
    Orientation m_orientation;
    int m_visibleSize;
    int m_totalSize;
    int m_currentPos;
    int m_lineStep;
    int m_pageStep;
    KWQSignal m_valueChanged;
};

#endif
