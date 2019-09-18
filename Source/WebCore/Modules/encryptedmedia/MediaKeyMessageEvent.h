/*
 * Copyright (C) 2016 Metrological Group B.V.
 * Copyright (C) 2016 Igalia S.L.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above
 *    copyright notice, this list of conditions and the following
 *    disclaimer in the documentation and/or other materials provided
 *    with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#if ENABLE(ENCRYPTED_MEDIA)

#include "Event.h"
#include "MediaKeyMessageEventInit.h"
#include "MediaKeyMessageType.h"
#include <JavaScriptCore/ArrayBuffer.h>

namespace WebCore {

class MediaKeyMessageEvent final : public Event {
    WTF_MAKE_ISO_ALLOCATED(MediaKeyMessageEvent);
public:
    using Type = MediaKeyMessageType;
    using Init = MediaKeyMessageEventInit;

    virtual ~MediaKeyMessageEvent();

    static Ref<MediaKeyMessageEvent> create(const AtomString& type, const MediaKeyMessageEventInit& initializer, IsTrusted isTrusted = IsTrusted::No)
    {
        return adoptRef(*new MediaKeyMessageEvent(type, initializer, isTrusted));
    }

    Type messageType() const { return m_messageType; }
    RefPtr<JSC::ArrayBuffer> message() const { return m_message; }

private:
    MediaKeyMessageEvent(const AtomString&, const MediaKeyMessageEventInit&, IsTrusted);

    // Event
    EventInterface eventInterface() const override;

    MediaKeyMessageType m_messageType;
    RefPtr<JSC::ArrayBuffer> m_message;
};

} // namespace WebCore

#endif // ENABLE(ENCRYPTED_MEDIA)
