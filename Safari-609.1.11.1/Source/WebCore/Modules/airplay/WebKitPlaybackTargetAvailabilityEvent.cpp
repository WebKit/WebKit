/*
 * Copyright (C) 2013 Apple Inc. All rights reserved.
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
#include "WebKitPlaybackTargetAvailabilityEvent.h"

#include <wtf/IsoMallocInlines.h>
#include <wtf/NeverDestroyed.h>

#if ENABLE(WIRELESS_PLAYBACK_TARGET)

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(WebKitPlaybackTargetAvailabilityEvent);

static const AtomString& stringForPlaybackTargetAvailability(bool available)
{
    static NeverDestroyed<AtomString> availableString("available", AtomString::ConstructFromLiteral);
    static NeverDestroyed<AtomString> notAvailableString("not-available", AtomString::ConstructFromLiteral);

    return available ? availableString : notAvailableString;
}

WebKitPlaybackTargetAvailabilityEvent::WebKitPlaybackTargetAvailabilityEvent(const AtomString& eventType, bool available)
    : Event(eventType, CanBubble::No, IsCancelable::No)
    , m_availability(stringForPlaybackTargetAvailability(available))
{
}

WebKitPlaybackTargetAvailabilityEvent::WebKitPlaybackTargetAvailabilityEvent(const AtomString& eventType, const Init& initializer, IsTrusted isTrusted)
    : Event(eventType, initializer, isTrusted)
    , m_availability(initializer.availability)
{
}

} // namespace WebCore

#endif // ENABLE(WIRELESS_PLAYBACK_TARGET)
