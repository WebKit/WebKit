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

#include <WebCore/ScriptTrackingPrivacyCategory.h>
#include <wtf/Noncopyable.h>
#include <wtf/OptionSet.h>
#include <wtf/RobinHoodHashMap.h>
#include <wtf/URL.h>
#include <wtf/Vector.h>
#include <wtf/text/WTFString.h>

namespace WebCore {
class SecurityOrigin;
}

namespace WebKit {

struct ScriptTrackingPrivacyHost {
    String hostName;
    WebCore::ScriptTrackingPrivacyFlags allowedCategories;
};

struct ScriptTrackingPrivacyRules {
    Vector<ScriptTrackingPrivacyHost> thirdPartyHosts;
    Vector<ScriptTrackingPrivacyHost> thirdPartyTopDomains;
    Vector<ScriptTrackingPrivacyHost> firstPartyHosts;
    Vector<ScriptTrackingPrivacyHost> firstPartyTopDomains;

    bool isEmpty() const
    {
        return thirdPartyHosts.isEmpty() && thirdPartyTopDomains.isEmpty() && firstPartyHosts.isEmpty() && firstPartyTopDomains.isEmpty();
    }
};

using HostToAllowedCategoriesMap = MemoryCompactRobinHoodHashMap<String, WebCore::ScriptTrackingPrivacyFlags>;

class ScriptTrackingPrivacyFilter {
    WTF_DEPRECATED_MAKE_FAST_ALLOCATED(ScriptTrackingPrivacyFilter);
    WTF_MAKE_NONCOPYABLE(ScriptTrackingPrivacyFilter);
public:
    ScriptTrackingPrivacyFilter(ScriptTrackingPrivacyRules&&);

    bool matches(const URL&, const WebCore::SecurityOrigin& topOrigin);
    bool shouldAllowAccess(const URL&, const WebCore::SecurityOrigin& topOrigin, WebCore::ScriptTrackingPrivacyCategory);

private:
    struct LookupResult {
        bool foundMatch { false };
        WebCore::ScriptTrackingPrivacyFlags allowedCategories;
    };
    LookupResult lookup(const URL&, const WebCore::SecurityOrigin& topOrigin);

    HostToAllowedCategoriesMap m_thirdPartyHosts;
    HostToAllowedCategoriesMap m_thirdPartyTopDomains;
    HostToAllowedCategoriesMap m_firstPartyHosts;
    HostToAllowedCategoriesMap m_firstPartyTopDomains;
    WebCore::ScriptTrackingPrivacyFlags m_categoriesWithAllowedHosts;
};

} // namespace WebKit
