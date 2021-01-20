/*
 * Copyright (C) 2015, 2016 Apple Inc. All rights reserved.
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

#if ENABLE(INDEXED_DATABASE)

#include "Event.h"
#include "IDBResourceIdentifier.h"
#include <wtf/Optional.h>

namespace WebCore {

class IDBVersionChangeEvent final : public Event {
    WTF_MAKE_ISO_ALLOCATED(IDBVersionChangeEvent);
public:
    static Ref<IDBVersionChangeEvent> create(uint64_t oldVersion, uint64_t newVersion, const AtomString& eventType)
    {
        return adoptRef(*new IDBVersionChangeEvent(IDBResourceIdentifier::emptyValue(), oldVersion, newVersion, eventType));
    }

    static Ref<IDBVersionChangeEvent> create(const IDBResourceIdentifier& requestIdentifier, uint64_t oldVersion, uint64_t newVersion, const AtomString& eventType)
    {
        return adoptRef(*new IDBVersionChangeEvent(requestIdentifier, oldVersion, newVersion, eventType));
    }

    struct Init : EventInit {
        uint64_t oldVersion { 0 };
        Optional<uint64_t> newVersion;
    };

    static Ref<IDBVersionChangeEvent> create(const AtomString& type, const Init& initializer, IsTrusted isTrusted = IsTrusted::No)
    {
        return adoptRef(*new IDBVersionChangeEvent(type, initializer, isTrusted));
    }

    const IDBResourceIdentifier& requestIdentifier() const { return m_requestIdentifier; }

    bool isVersionChangeEvent() const final { return true; }

    uint64_t oldVersion() const { return m_oldVersion; }
    Optional<uint64_t> newVersion() const { return m_newVersion; }

private:
    IDBVersionChangeEvent(const IDBResourceIdentifier& requestIdentifier, uint64_t oldVersion, uint64_t newVersion, const AtomString& eventType);
    IDBVersionChangeEvent(const AtomString&, const Init&, IsTrusted);

    EventInterface eventInterface() const;

    IDBResourceIdentifier m_requestIdentifier;
    uint64_t m_oldVersion;
    Optional<uint64_t> m_newVersion;
};

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_BEGIN(WebCore::IDBVersionChangeEvent)
    static bool isType(const WebCore::Event& event) { return event.isVersionChangeEvent(); }
SPECIALIZE_TYPE_TRAITS_END()

#endif // ENABLE(INDEXED_DATABASE)
