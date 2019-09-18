/*
 * Copyright (C) 2016 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#if ENABLE(MEDIA_STREAM)

#include "Event.h"
#include "OverconstrainedError.h"
#include <wtf/text/AtomString.h>

namespace WebCore {

class OverconstrainedErrorEvent final : public Event {
    WTF_MAKE_ISO_ALLOCATED(OverconstrainedErrorEvent);
public:
    virtual ~OverconstrainedErrorEvent() = default;

    static Ref<OverconstrainedErrorEvent> create(const AtomString& type, CanBubble canBubble, IsCancelable cancelable, OverconstrainedError* error)
    {
        return adoptRef(*new OverconstrainedErrorEvent(type, canBubble, cancelable, error));
    }

    struct Init : EventInit {
        RefPtr<OverconstrainedError> error;
    };

    static Ref<OverconstrainedErrorEvent> create(const AtomString& type, const Init& initializer, IsTrusted isTrusted = IsTrusted::No)
    {
        return adoptRef(*new OverconstrainedErrorEvent(type, initializer, isTrusted));
    }

    OverconstrainedError* error() const { return m_error.get(); }
    EventInterface eventInterface() const override { return OverconstrainedErrorEventInterfaceType; }

private:
    explicit OverconstrainedErrorEvent(const AtomString& type, CanBubble canBubble, IsCancelable cancelable, OverconstrainedError* error)
        : Event(type, canBubble, cancelable)
        , m_error(error)
    {
    }
    OverconstrainedErrorEvent(const AtomString& type, const Init& initializer, IsTrusted isTrusted)
        : Event(type, initializer, isTrusted)
        , m_error(initializer.error)
    {
    }

    RefPtr<OverconstrainedError> m_error;
};

} // namespace WebCore

#endif // ENABLE(MEDIA_STREAM)
