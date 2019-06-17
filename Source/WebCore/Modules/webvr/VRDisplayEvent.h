/*
 * Copyright (C) 2017-2018 Igalia S.L. All rights reserved.
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
#pragma once

#include "Event.h"
#include "VRDisplay.h"
#include "VRDisplayEventReason.h"

namespace WebCore {

class VRDisplayEvent final : public Event {
public:
    static Ref<VRDisplayEvent> create(const AtomString& type, const RefPtr<VRDisplay>& display, Optional<VRDisplayEventReason>&& reason)
    {
        return adoptRef(*new VRDisplayEvent(type, display, WTFMove(reason)));
    }

    struct Init : EventInit {
        RefPtr<VRDisplay> display;
        Optional<VRDisplayEventReason> reason;
    };

    static Ref<VRDisplayEvent> create(const AtomString& type, const Init& initializer, IsTrusted isTrusted = IsTrusted::No)
    {
        return adoptRef(*new VRDisplayEvent(type, initializer, isTrusted));
    }

    virtual ~VRDisplayEvent();

    RefPtr<VRDisplay> display() const { return m_display; }
    const Optional<VRDisplayEventReason>& reason() const { return m_reason; }

private:
    VRDisplayEvent(const AtomString&, const Init&, IsTrusted);
    VRDisplayEvent(const AtomString&, const RefPtr<VRDisplay>&, Optional<VRDisplayEventReason>&&);

    // Event
    EventInterface eventInterface() const override;

    RefPtr<VRDisplay> m_display;
    Optional<VRDisplayEventReason> m_reason;
};

} // namespace WebCore
