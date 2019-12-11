/*
 * Copyright (C) 2001 Peter Kelly (pmk@post.com)
 * Copyright (C) 2001 Tobias Anton (anton@stud.fbi.fh-darmstadt.de)
 * Copyright (C) 2006 Samuel Weinig (sam.weinig@gmail.com)
 * Copyright (C) 2003, 2005, 2006, 2008 Apple Inc. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include "config.h"
#include "UIEvent.h"

#include "Node.h"
#include <wtf/IsoMallocInlines.h>

namespace WebCore {
    
WTF_MAKE_ISO_ALLOCATED_IMPL(UIEvent);

UIEvent::UIEvent()
    : m_detail(0)
{
}

UIEvent::UIEvent(const AtomString& eventType, CanBubble canBubble, IsCancelable isCancelable, IsComposed isComposed, RefPtr<WindowProxy>&& viewArg, int detailArg)
    : Event(eventType, canBubble, isCancelable, isComposed)
    , m_view(WTFMove(viewArg))
    , m_detail(detailArg)
{
}

UIEvent::UIEvent(const AtomString& eventType, CanBubble canBubble, IsCancelable isCancelable, IsComposed isComposed, MonotonicTime timestamp, RefPtr<WindowProxy>&& viewArg, int detailArg, IsTrusted isTrusted)
    : Event(eventType, canBubble, isCancelable, isComposed, timestamp, isTrusted)
    , m_view(WTFMove(viewArg))
    , m_detail(detailArg)
{
}

UIEvent::UIEvent(const AtomString& eventType, const UIEventInit& initializer)
    : Event(eventType, initializer, IsTrusted::No)
    , m_view(initializer.view.get())
    , m_detail(initializer.detail)
{
}

UIEvent::~UIEvent() = default;

void UIEvent::initUIEvent(const AtomString& typeArg, bool canBubbleArg, bool cancelableArg, RefPtr<WindowProxy>&& viewArg, int detailArg)
{
    if (isBeingDispatched())
        return;

    initEvent(typeArg, canBubbleArg, cancelableArg);

    m_view = viewArg;
    m_detail = detailArg;
}

bool UIEvent::isUIEvent() const
{
    return true;
}

EventInterface UIEvent::eventInterface() const
{
    return UIEventInterfaceType;
}

int UIEvent::layerX()
{
    return 0;
}

int UIEvent::layerY()
{
    return 0;
}

int UIEvent::pageX() const
{
    return 0;
}

int UIEvent::pageY() const
{
    return 0;
}

int UIEvent::which() const
{
    return 0;
}

} // namespace WebCore
