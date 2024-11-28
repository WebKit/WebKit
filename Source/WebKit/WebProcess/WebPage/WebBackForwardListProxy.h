/*
 * Copyright (C) 2010, 2011 Apple Inc. All rights reserved.
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

#include "WebBackForwardListCounts.h"
#include <WebCore/BackForwardClient.h>
#include <WebCore/BackForwardItemIdentifier.h>
#include <WebCore/FrameIdentifier.h>
#include <WebCore/PageIdentifier.h>

namespace WebKit {

class WebPage;

class WebBackForwardListProxy : public WebCore::BackForwardClient {
public: 
    static Ref<WebBackForwardListProxy> create(WebPage& page) { return adoptRef(*new WebBackForwardListProxy(page)); }

    static void removeItem(const WebCore::BackForwardItemIdentifier&);

    void clearCachedListCounts();

private:
    WebBackForwardListProxy(WebPage&);

    void addItem(WebCore::FrameIdentifier, Ref<WebCore::HistoryItem>&&) override;
    void setChildItem(WebCore::BackForwardItemIdentifier, Ref<WebCore::HistoryItem>&&) final;

    void goToItem(WebCore::HistoryItem&) override;
    void goToProvisionalItem(const WebCore::HistoryItem&) final;
    void clearProvisionalItem(const WebCore::HistoryItem&) final;

    RefPtr<WebCore::HistoryItem> itemAtIndex(int, WebCore::FrameIdentifier) override;
    unsigned backListCount() const override;
    unsigned forwardListCount() const override;
    bool containsItem(const WebCore::HistoryItem&) const final;
    const WebBackForwardListCounts& cacheListCountsIfNecessary() const;

    void close() override;

    WeakPtr<WebPage> m_page;
    mutable std::optional<WebBackForwardListCounts> m_cachedBackForwardListCounts;
};

} // namespace WebKit
