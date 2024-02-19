/*
 * Copyright (C) 2023 Igalia S.L. All rights reserved.
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
#include "NavigateEvent.h"

#include <wtf/IsoMallocInlines.h>

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(NavigateEvent);

NavigateEvent::NavigateEvent(const AtomString& type, const NavigateEvent::Init& init)
    : Event(type, CanBubble::No, IsCancelable::No)
    , m_navigationType(init.navigationType)
    , m_destination(init.destination)
    , m_signal(init.signal)
    , m_formData(init.formData)
    , m_downloadRequest(init.downloadRequest)
    , m_info(init.info)
    , m_canIntercept(init.canIntercept)
    , m_userInitiated(init.userInitiated)
    , m_hashChange(init.hashChange)
    , m_hasUAVisualTransition(init.hasUAVisualTransition)
{
}

Ref<NavigateEvent> NavigateEvent::create(const AtomString& type, const NavigateEvent::Init& init)
{
    return adoptRef(*new NavigateEvent(type, init));
}

void NavigateEvent::intercept(NavigationInterceptOptions&&)
{
}

void NavigateEvent::scroll()
{
}

EventInterface NavigateEvent::eventInterface() const
{
    return NavigateEventInterfaceType;
}

} // namespace WebCore
