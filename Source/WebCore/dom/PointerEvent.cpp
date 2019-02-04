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

PointerEvent::~PointerEvent() = default;

EventInterface PointerEvent::eventInterface() const
{
    return PointerEventInterfaceType;
}

} // namespace WebCore

#endif // ENABLE(POINTER_EVENTS)
