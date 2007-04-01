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

#include "config.h"
#include "ScrollBar.h"

#include <algorithm>

using std::max;
using std::min;

namespace WebCore {

Scrollbar::Scrollbar(ScrollbarClient* client, ScrollbarOrientation orientation, ScrollbarControlSize controlSize)
    : m_client(client)
    , m_orientation(orientation)
    , m_controlSize(controlSize)
    , m_visibleSize(0)
    , m_totalSize(0)
    , m_currentPos(0)
    , m_lineStep(0)
    , m_pageStep(0)
    , m_pixelStep(1)
{
}

bool Scrollbar::setValue(int v)
{
    v = max(min(v, m_totalSize - m_visibleSize), 0);
    if (value() == v)
        return false; // Our value stayed the same.
    m_currentPos = v;

    updateThumbPosition();

    if (client())
        client()->valueChanged(this);
    
    return true;
}

void Scrollbar::setProportion(int visibleSize, int totalSize)
{
    if (visibleSize == m_visibleSize && totalSize == m_totalSize)
        return;

    m_visibleSize = visibleSize;
    m_totalSize = totalSize;

    updateThumbProportion();
}

void Scrollbar::setSteps(int lineStep, int pageStep, int pixelsPerStep)
{
    m_lineStep = lineStep;
    m_pageStep = pageStep;
    m_pixelStep = 1.0f / pixelsPerStep;
}

bool Scrollbar::scroll(ScrollDirection direction, ScrollGranularity granularity, float multiplier)
{
    float step = 0;
    if ((direction == ScrollUp && m_orientation == VerticalScrollbar) || (direction == ScrollLeft && m_orientation == HorizontalScrollbar))
        step = -1;
    else if ((direction == ScrollDown && m_orientation == VerticalScrollbar) || (direction == ScrollRight && m_orientation == HorizontalScrollbar)) 
        step = 1;
    
    if (granularity == ScrollByLine)
        step *= m_lineStep;
    else if (granularity == ScrollByPage)
        step *= m_pageStep;
    else if (granularity == ScrollByDocument)
        step *= m_totalSize;
    else if (granularity == ScrollByPixel)
        step *= m_pixelStep;
        
    float newPos = m_currentPos + step * multiplier;
    float maxPos = m_totalSize - m_visibleSize;
    newPos = max(min(newPos, maxPos), 0.0f);

    if (newPos == m_currentPos)
        return false;
    
    int oldValue = value();
    m_currentPos = newPos;
    updateThumbPosition();
    
    if (value() != oldValue && client())
        client()->valueChanged(this);
    
    // return true even if the integer value did not change so that scroll event gets eaten
    return true;
}
}
