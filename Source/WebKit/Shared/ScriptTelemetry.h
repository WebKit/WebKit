/*
 * Copyright (C) 2024 Apple Inc. All rights reserved.
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

#include <wtf/Noncopyable.h>
#include <wtf/RobinHoodHashSet.h>
#include <wtf/URL.h>
#include <wtf/Vector.h>
#include <wtf/text/WTFString.h>

namespace WebCore {
class SecurityOrigin;
}

namespace WebKit {

struct ScriptTelemetryRules {
    Vector<String> thirdPartyHosts;
    Vector<String> thirdPartyTopDomains;
    Vector<String> firstPartyHosts;
    Vector<String> firstPartyTopDomains;

    bool isEmpty() const
    {
        return thirdPartyHosts.isEmpty() && thirdPartyTopDomains.isEmpty() && firstPartyHosts.isEmpty() && firstPartyTopDomains.isEmpty();
    }
};

class ScriptTelemetryFilter {
    WTF_MAKE_FAST_ALLOCATED;
    WTF_MAKE_NONCOPYABLE(ScriptTelemetryFilter);
public:
    ScriptTelemetryFilter(ScriptTelemetryRules&&);

    bool matches(const URL&, const WebCore::SecurityOrigin& topOrigin);

private:
    MemoryCompactRobinHoodHashSet<String> m_thirdPartyHosts;
    MemoryCompactRobinHoodHashSet<String> m_thirdPartyTopDomains;
    MemoryCompactRobinHoodHashSet<String> m_firstPartyHosts;
    MemoryCompactRobinHoodHashSet<String> m_firstPartyTopDomains;
};

} // namespace WebKit
