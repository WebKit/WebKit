/*
 * Copyright (C) 2018 Apple Inc. All rights reserved.
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
#include "PointerEvent.h"

#include "EventNames.h"
#include "Node.h"
#include <wtf/IsoMallocInlines.h>

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(PointerEvent);

static AtomString pointerEventType(const AtomString& mouseEventType)
{
    auto& names = eventNames();
    if (mouseEventType == names.mousedownEvent)
        return names.pointerdownEvent;
    if (mouseEventType == names.mouseoverEvent)
        return names.pointeroverEvent;
    if (mouseEventType == names.mouseenterEvent)
        return names.pointerenterEvent;
    if (mouseEventType == names.mousemoveEvent)
        return names.pointermoveEvent;
    if (mouseEventType == names.mouseleaveEvent)
        return names.pointerleaveEvent;
    if (mouseEventType == names.mouseoutEvent)
        return names.pointeroutEvent;
    if (mouseEventType == names.mouseupEvent)
        return names.pointerupEvent;

    return nullAtom();
}

RefPtr<PointerEvent> PointerEvent::create(short button, const MouseEvent& mouseEvent, PointerID pointerId, const String& pointerType)
{
    auto type = pointerEventType(mouseEvent.type());
    if (type.isEmpty())
        return nullptr;

    return create(type, button, mouseEvent, pointerId, pointerType);
}

Ref<PointerEvent> PointerEvent::create(const AtomString& type, short button, const MouseEvent& mouseEvent, PointerID pointerId, const String& pointerType)
{
    return adoptRef(*new PointerEvent(type, button, mouseEvent, pointerId, pointerType));
}

Ref<PointerEvent> PointerEvent::create(const AtomString& type, PointerID pointerId, const String& pointerType, IsPrimary isPrimary)
{
    return adoptRef(*new PointerEvent(type, pointerId, pointerType, isPrimary));
}

PointerEvent::PointerEvent() = default;

PointerEvent::PointerEvent(const AtomString& type, Init&& initializer)
    : MouseEvent(type, initializer)
    , m_pointerId(initializer.pointerId)
    , m_width(initializer.width)
    , m_height(initializer.height)
    , m_pressure(initializer.pressure)
    , m_tangentialPressure(initializer.tangentialPressure)
    , m_tiltX(initializer.tiltX)
    , m_tiltY(initializer.tiltY)
    , m_twist(initializer.twist)
    , m_pointerType(initializer.pointerType)
    , m_isPrimary(initializer.isPrimary)
{
}

PointerEvent::PointerEvent(const AtomString& type, short button, const MouseEvent& mouseEvent, PointerID pointerId, const String& pointerType)
    : MouseEvent(type, typeCanBubble(type), typeIsCancelable(type), typeIsComposed(type), mouseEvent.view(), mouseEvent.detail(), mouseEvent.screenLocation(),
        { mouseEvent.clientX(), mouseEvent.clientY() }, mouseEvent.movementX(), mouseEvent.movementY(), mouseEvent.modifierKeys(), button, mouseEvent.buttons(),
        mouseEvent.syntheticClickType(), mouseEvent.relatedTarget())
    , m_pointerId(pointerId)
    , m_pointerType(pointerType)
    , m_isPrimary(true)
{
}

PointerEvent::PointerEvent(const AtomString& type, PointerID pointerId, const String& pointerType, IsPrimary isPrimary)
    : MouseEvent(type, typeCanBubble(type), typeIsCancelable(type), typeIsComposed(type), nullptr, 0, { }, { }, 0, 0, { }, 0, 0, 0, nullptr)
    , m_pointerId(pointerId)
    , m_pointerType(pointerType)
    , m_isPrimary(isPrimary == IsPrimary::Yes)
{
}

PointerEvent::~PointerEvent() = default;

EventInterface PointerEvent::eventInterface() const
{
    return PointerEventInterfaceType;
}

} // namespace WebCore
