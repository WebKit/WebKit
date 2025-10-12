/*
 * Copyright (C) 2015-2024 Apple Inc. All rights reserved.
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

#include <WebCore/PlatformContentFilter.h>
#include <wtf/Compiler.h>
#include <wtf/RetainPtr.h>
#include <wtf/TZoneMalloc.h>
#include <wtf/UniqueRef.h>

OBJC_CLASS NSData;
OBJC_CLASS WebFilterEvaluator;

namespace WebCore {

class ParentalControlsURLFilter;

class ParentalControlsContentFilter final : public PlatformContentFilter {
    WTF_MAKE_TZONE_ALLOCATED(ParentalControlsContentFilter);

public:
    static Ref<ParentalControlsContentFilter> create(const PlatformContentFilter::FilterParameters&);

    void willSendRequest(ResourceRequest&, const ResourceResponse&) override { }
    void responseReceived(const ResourceResponse&) override;
    void addData(const SharedBuffer&) override;
    void finishedAddingData() override;
    Ref<FragmentedSharedBuffer> replacementData() const override;
#if ENABLE(CONTENT_FILTERING)
    ContentFilterUnblockHandler unblockHandler() const override;
#endif
    
#if HAVE(WEBCONTENTRESTRICTIONS)
    void didReceiveAllowDecisionOnQueue(bool isAllowed, NSData *);
#endif

private:
    explicit ParentalControlsContentFilter(const PlatformContentFilter::FilterParameters&);
    bool enabled() const;

    void updateFilterState();
#if HAVE(WEBCONTENTRESTRICTIONS)
    void updateFilterStateOnMain();
#endif

    RetainPtr<WebFilterEvaluator> m_webFilterEvaluator;
    RetainPtr<NSData> m_replacementData;

#if HAVE(WEBCONTENTRESTRICTIONS)
    bool m_usesWebContentRestrictions { false };
    std::optional<URL> m_evaluatedURL;
    Lock m_resultLock;
    Condition m_resultCondition;
    std::optional<bool> m_isAllowdByWebContentRestrictions WTF_GUARDED_BY_LOCK(m_resultLock);
    RetainPtr<NSData> m_webContentRestrictionsReplacementData WTF_GUARDED_BY_LOCK(m_resultLock);
#endif
#if HAVE(WEBCONTENTRESTRICTIONS_PATH_SPI)
    String m_webContentRestrictionsConfigurationPath;
#endif
};
    
} // namespace WebCore
