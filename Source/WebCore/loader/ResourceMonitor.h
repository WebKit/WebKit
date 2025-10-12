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

#if ENABLE(CONTENT_EXTENSIONS)

#include <WebCore/ResourceMonitorChecker.h>
#include <wtf/CheckedArithmetic.h>
#include <wtf/RefCountedAndCanMakeWeakPtr.h>

namespace WebCore {

class LocalFrame;

class ResourceMonitor final : public RefCountedAndCanMakeWeakPtr<ResourceMonitor> {
public:
    using Eligibility = ResourceMonitorEligibility;
    enum class UsageLevel : uint8_t {
        Empty = 0,
        Low = 20,
        Medium = 60,
        High = 80,
        Critical = 100,
    };

    static Ref<ResourceMonitor> create(LocalFrame&);
    WEBCORE_EXPORT ~ResourceMonitor();

    Eligibility eligibility() const { return m_eligibility; }
    void setEligibility(Eligibility);
    bool isEligible() const { return eligibility() == Eligibility::Eligible; }

    void setDocumentURL(URL&&);
    WEBCORE_EXPORT void addNetworkUsage(size_t);
    size_t networkUsageThreshold() const { return m_networkUsageThreshold; }
    WEBCORE_EXPORT UsageLevel networkUsageLevel() const;

    void updateNetworkUsageThreshold(size_t);

private:
    explicit ResourceMonitor(LocalFrame&);

    void didReceiveResponse(const URL&, OptionSet<ContentExtensions::ResourceType>);
    void continueAfterDidReceiveEligibility(Eligibility, const URL&, OptionSet<ContentExtensions::ResourceType>);
    void checkNetworkUsageExcessIfNecessary();
    ResourceMonitor* parentResourceMonitorIfExists() const;

    WeakPtr<LocalFrame> m_frame;
    URL m_frameURL;
    size_t m_networkUsageThreshold;
    CheckedSize m_networkUsage;
    Eligibility m_eligibility { Eligibility::Unsure };
    bool m_networkUsageExceed { false };
};

} // namespace WebCore

#endif
