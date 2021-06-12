/*
 * Copyright (C) 2015 Apple Inc. All rights reserved.
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

#include "MockContentFilterSettings.h"
#include "PlatformContentFilter.h"
#include <wtf/UniqueRef.h>

namespace WebCore {

class MockContentFilter final : public PlatformContentFilter {
    friend UniqueRef<MockContentFilter> WTF::makeUniqueRefWithoutFastMallocCheck<MockContentFilter>();

public:
    static void ensureInstalled();
    static UniqueRef<MockContentFilter> create();

    void willSendRequest(ResourceRequest&, const ResourceResponse&) override;
    void responseReceived(const ResourceResponse&) override;
    void addData(const uint8_t* data, int length) override;
    void finishedAddingData() override;
    Ref<SharedBuffer> replacementData() const override;
#if ENABLE(CONTENT_FILTERING)
    ContentFilterUnblockHandler unblockHandler() const override;
#endif
    String unblockRequestDeniedScript() const override;

private:
    static bool enabled();

    MockContentFilter() = default;
    void maybeDetermineStatus(MockContentFilterSettings::DecisionPoint);

    Vector<char> m_replacementData;
};

} // namespace WebCore
