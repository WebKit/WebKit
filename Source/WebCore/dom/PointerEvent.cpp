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

#if ENABLE(POINTER_EVENTS)

#import "EventNames.h"

namespace WebCore {

const String& PointerEvent::mousePointerType()
{
    static NeverDestroyed<const String> mouseType(MAKE_STATIC_STRING_IMPL("mouse"));
    return mouseType;
}

const String& PointerEvent::penPointerType()
{
    static NeverDestroyed<const String> penType(MAKE_STATIC_STRING_IMPL("pen"));
    return penType;
}

const String& PointerEvent::touchPointerType()
{
    static NeverDestroyed<const String> touchType(MAKE_STATIC_STRING_IMPL("touch"));
    return touchType;
}

static AtomicString pointerEventType(const AtomicString& mouseEventType)
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

PointerEvent::PointerEvent() = default;

PointerEvent::PointerEvent(const AtomicString& type, Init&& initializer)
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

RefPtr<PointerEvent> PointerEvent::create(const MouseEvent& mouseEvent)
{
    auto type = pointerEventType(mouseEvent.type());
    if (type.isEmpty())
        return nullptr;

    auto isEnterOrLeave = type == eventNames().pointerenterEvent || type == eventNames().pointerleaveEvent;

    PointerEvent::Init init;
    init.bubbles = !isEnterOrLeave;
    init.cancelable = !isEnterOrLeave;
    init.composed = !isEnterOrLeave;
    init.view = mouseEvent.view();
    init.ctrlKey = mouseEvent.ctrlKey();
    init.shiftKey = mouseEvent.shiftKey();
    init.altKey = mouseEvent.altKey();
    init.metaKey = mouseEvent.metaKey();
    init.modifierAltGraph = mouseEvent.altGraphKey();
    init.modifierCapsLock = mouseEvent.capsLockKey();
    init.screenX = mouseEvent.screenX();
    init.screenY = mouseEvent.screenY();
    init.clientX = mouseEvent.clientX();
    init.clientY = mouseEvent.clientY();
    init.button = mouseEvent.button();
    init.buttons = mouseEvent.buttons();
    init.relatedTarget = mouseEvent.relatedTarget();
    init.isPrimary = true;

    return PointerEvent::create(type, WTFMove(init));
}

PointerEvent::~PointerEvent() = default;

EventInterface PointerEvent::eventInterface() const
{
    return PointerEventInterfaceType;
}

} // namespace WebCore

#endif // ENABLE(POINTER_EVENTS)
