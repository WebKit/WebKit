/*
 * Copyright (C) 2023 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "BackgroundFetchEvent.h"

#include <wtf/IsoMallocInlines.h>

#if ENABLE(SERVICE_WORKER)

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(BackgroundFetchEvent);

Ref<BackgroundFetchEvent> BackgroundFetchEvent::create(const AtomString& type, Init&& init, IsTrusted isTrusted)
{
    auto registration = init.registration;
    return adoptRef(*new BackgroundFetchEvent(type, WTFMove(init), WTFMove(registration), isTrusted));
}

BackgroundFetchEvent::BackgroundFetchEvent(const AtomString& type, ExtendableEventInit&& eventInit, RefPtr<BackgroundFetchRegistration>&& registration, IsTrusted isTrusted)
    : ExtendableEvent(type, WTFMove(eventInit), isTrusted)
    , m_registration(WTFMove(registration))
{
}

BackgroundFetchEvent::~BackgroundFetchEvent()
{
}

RefPtr<BackgroundFetchRegistration> BackgroundFetchEvent::registration() const
{
    return m_registration;
}

} // namespace WebCore

#endif // ENABLE(SERVICE_WORKER)


