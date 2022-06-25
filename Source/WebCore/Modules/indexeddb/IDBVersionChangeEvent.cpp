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

#include "config.h"
#include "IDBVersionChangeEvent.h"

#include <wtf/IsoMallocInlines.h>

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(IDBVersionChangeEvent);

IDBVersionChangeEvent::IDBVersionChangeEvent(const IDBResourceIdentifier& requestIdentifier, uint64_t oldVersion, uint64_t newVersion, const AtomString& name)
    : Event(name, CanBubble::No, IsCancelable::No)
    , m_requestIdentifier(requestIdentifier)
    , m_oldVersion(oldVersion)
{
    if (newVersion)
        m_newVersion = newVersion;
    else
        m_newVersion = std::nullopt;
}

IDBVersionChangeEvent::IDBVersionChangeEvent(const AtomString& name, const Init& init, IsTrusted isTrusted)
    : Event(name, init, isTrusted)
    , m_oldVersion(init.oldVersion)
    , m_newVersion(init.newVersion)
{
}

EventInterface IDBVersionChangeEvent::eventInterface() const
{
    return IDBVersionChangeEventInterfaceType;
}

} // namespace WebCore
