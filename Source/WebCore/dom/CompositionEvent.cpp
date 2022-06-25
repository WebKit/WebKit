/*
 * Copyright (C) 2009 Apple Inc. All rights reserved.
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
 *
 */

#include "config.h"
#include "CompositionEvent.h"

#include <wtf/IsoMallocInlines.h>

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(CompositionEvent);

CompositionEvent::CompositionEvent() = default;

CompositionEvent::CompositionEvent(const AtomString& type, RefPtr<WindowProxy>&& view, const String& data)
    : UIEvent(type, CanBubble::Yes, IsCancelable::Yes, IsComposed::Yes, WTFMove(view), 0)
    , m_data(data)
{
}

CompositionEvent::CompositionEvent(const AtomString& type, const Init& initializer)
    : UIEvent(type, initializer)
    , m_data(initializer.data)
{
}

CompositionEvent::~CompositionEvent() = default;

void CompositionEvent::initCompositionEvent(const AtomString& type, bool canBubble, bool cancelable, RefPtr<WindowProxy>&& view, const String& data)
{
    if (isBeingDispatched())
        return;

    initUIEvent(type, canBubble, cancelable, WTFMove(view), 0);

    m_data = data;
}

EventInterface CompositionEvent::eventInterface() const
{
    return CompositionEventInterfaceType;
}

bool CompositionEvent::isCompositionEvent() const
{
    return true;
}

} // namespace WebCore
