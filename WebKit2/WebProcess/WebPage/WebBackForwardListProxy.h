/*
 * Copyright (C) 2010 Apple Inc. All rights reserved.
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

#ifndef WebBackForwardListProxy_h
#define WebBackForwardListProxy_h

#include <WebCore/BackForwardList.h>

namespace WebKit {

class WebPage;

class WebBackForwardListProxy : public WebCore::BackForwardList {
public: 
    static PassRefPtr<WebBackForwardListProxy> create(WebPage* page) { return adoptRef(new WebBackForwardListProxy(page)); }
    ~WebBackForwardListProxy();

    static WebCore::HistoryItem* itemForID(uint64_t);

    void addItem(PassRefPtr<WebCore::HistoryItem>);
    void goBack();
    void goForward();
    void goToItem(WebCore::HistoryItem*);
        
    WebCore::HistoryItem* backItem();
    WebCore::HistoryItem* currentItem();
    WebCore::HistoryItem* forwardItem();
    WebCore::HistoryItem* itemAtIndex(int);

    void backListWithLimit(int, WebCore::HistoryItemVector&);
    void forwardListWithLimit(int, WebCore::HistoryItemVector&);

    int capacity();
    void setCapacity(int);
    bool enabled();
    void setEnabled(bool);
    int backListCount();
    int forwardListCount();
    bool containsItem(WebCore::HistoryItem*);

    void close();
    bool closed();
    
    void removeItem(WebCore::HistoryItem*);
    WebCore::HistoryItemVector& entries();
    
#if ENABLE(WML)
    void clearWMLPageHistory();
#endif

private:
    WebBackForwardListProxy(WebPage*);
    
    WebPage* m_page;

    unsigned m_capacity;
    bool m_closed;
    bool m_enabled;
};

} // namespace WebKit

#endif // WebBackForwardListProxy_h
