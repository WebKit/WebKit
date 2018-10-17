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

#include <WebCore/BackForwardClient.h>
#include <wtf/HashSet.h>

namespace WebCore {
struct BackForwardItemIdentifier;
}

namespace WebKit {

class WebPage;

class WebBackForwardListProxy : public WebCore::BackForwardClient {
public: 
    static Ref<WebBackForwardListProxy> create(WebPage& page) { return adoptRef(*new WebBackForwardListProxy(page)); }

    static WebCore::HistoryItem* itemForID(const WebCore::BackForwardItemIdentifier&);
    static void removeItem(const WebCore::BackForwardItemIdentifier&);

    enum class OverwriteExistingItem {
        Yes,
        No
    };
    void addItemFromUIProcess(const WebCore::BackForwardItemIdentifier&, Ref<WebCore::HistoryItem>&&, uint64_t pageID, OverwriteExistingItem);

    void clear();

private:
    WebBackForwardListProxy(WebPage&);

    void addItem(Ref<WebCore::HistoryItem>&&) override;

    void goToItem(WebCore::HistoryItem&) override;
        
    RefPtr<WebCore::HistoryItem> itemAtIndex(int) override;
    unsigned backListCount() const override;
    unsigned forwardListCount() const override;

    void close() override;

    WebPage* m_page;
};

} // namespace WebKit
