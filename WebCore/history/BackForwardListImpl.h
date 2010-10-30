/*
 * Copyright (C) 2006, 2010 Apple Inc. All rights reserved.
 * Copyright (C) 2008 Torch Mobile Inc. All rights reserved. (http://www.torchmobile.com/)
 * Copyright (C) 2009 Google, Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#ifndef BackForwardListImpl_h
#define BackForwardListImpl_h

#include "BackForwardList.h"
#include <wtf/HashSet.h>
#include <wtf/Vector.h>

namespace WebCore {

class Page;

typedef Vector<RefPtr<HistoryItem> > HistoryItemVector;
typedef HashSet<RefPtr<HistoryItem> > HistoryItemHashSet;

// FIXME: Change Chromium to use its own BackForwardList implementation
// and not use BackForwardListImpl at all, then remove this
// BackForwardListClient feature entirely and just don't use this
// class on Chromium.
#if PLATFORM(CHROMIUM)
// In the Chromium port, the back/forward list is managed externally.
// See BackForwardListChromium.cpp
class BackForwardListClient {
public:
    virtual ~BackForwardListClient() { }
    virtual void addItem(PassRefPtr<HistoryItem>) = 0;
    virtual void goToItem(HistoryItem*) = 0;
    virtual HistoryItem* itemAtIndex(int) = 0;
    virtual int backListCount() = 0;
    virtual int forwardListCount() = 0;
    virtual void close() = 0;
};
#endif

// FIXME: After renaming BackForwardList to BackForwardClient,
// rename this to BackForwardList.
class BackForwardListImpl : public BackForwardList {
public: 
    static PassRefPtr<BackForwardListImpl> create(Page* page) { return adoptRef(new BackForwardListImpl(page)); }
    virtual ~BackForwardListImpl();

#if PLATFORM(CHROMIUM)
    // Must be called before any other methods. 
    void setClient(BackForwardListClient* client) { m_client = client; }
#endif
    
    Page* page() { return m_page; }
    
    virtual void addItem(PassRefPtr<HistoryItem>);
    void goBack();
    void goForward();
    virtual void goToItem(HistoryItem*);
        
    HistoryItem* backItem();
    HistoryItem* currentItem();
    HistoryItem* forwardItem();
    virtual HistoryItem* itemAtIndex(int);

    void backListWithLimit(int, HistoryItemVector&);
    void forwardListWithLimit(int, HistoryItemVector&);

    int capacity();
    void setCapacity(int);
    bool enabled();
    void setEnabled(bool);
    virtual int backListCount();
    virtual int forwardListCount();
    bool containsItem(HistoryItem*);

    virtual void close();
    bool closed();
    
    void removeItem(HistoryItem*);
    HistoryItemVector& entries();
    
#if ENABLE(WML)
    virtual void clearWMLPageHistory();
#endif

private:
    BackForwardListImpl(Page*);

    virtual bool isActive() { return enabled() && capacity(); }

    Page* m_page;
#if PLATFORM(CHROMIUM) 
    BackForwardListClient* m_client;
#else
    HistoryItemVector m_entries;
    HistoryItemHashSet m_entryHash;
    unsigned m_current;
#endif
    unsigned m_capacity;
    bool m_closed;
    bool m_enabled;
};
    
} // namespace WebCore

#endif // BackForwardListImpl_h
