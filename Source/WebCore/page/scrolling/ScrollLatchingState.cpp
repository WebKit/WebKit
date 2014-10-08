/*
 * Copyright (C) 2014 Apple Inc. All rights reserved.
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

#include "config.h"
#include "ScrollLatchingState.h"

#include "Element.h"

namespace WebCore {

ScrollLatchingState::ScrollLatchingState()
    : m_frame(nullptr)
    , m_widgetIsLatched(false)
    , m_startedGestureAtScrollLimit(false)
{
}
    
ScrollLatchingState::~ScrollLatchingState()
{
}

void ScrollLatchingState::clear()
{
    m_wheelEventElement = nullptr;
    m_frame = nullptr;
    m_scrollableContainer = nullptr;
    m_widgetIsLatched = false;
    m_previousWheelScrolledElement = nullptr;
}

void ScrollLatchingState::setWheelEventElement(PassRefPtr<Element> element)
{
    m_wheelEventElement = element;
}

void ScrollLatchingState::setWidgetIsLatched(bool isOverWidget)
{
    m_widgetIsLatched = isOverWidget;
}

void ScrollLatchingState::setPreviousWheelScrolledElement(PassRefPtr<Element> element)
{
    m_previousWheelScrolledElement = element;
}

void ScrollLatchingState::setScrollableContainer(PassRefPtr<ContainerNode> node)
{
    m_scrollableContainer = node;
}

}
