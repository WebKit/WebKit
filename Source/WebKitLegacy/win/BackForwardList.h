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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#pragma once

#include <WebCore/BackForwardClient.h>
#include <wtf/HashSet.h>
#include <wtf/Vector.h>

typedef Vector<Ref<WebCore::HistoryItem>> HistoryItemVector;
typedef HashSet<RefPtr<WebCore::HistoryItem>> HistoryItemHashSet;

class BackForwardList : public WebCore::BackForwardClient {
public: 
    static Ref<BackForwardList> create() { return adoptRef(*new BackForwardList()); }
    virtual ~BackForwardList();

    void addItem(Ref<WebCore::HistoryItem>&&) override;
    void goBack();
    void goForward();
    void goToItem(WebCore::HistoryItem&) override;
        
    WebCore::HistoryItem* backItem();
    WebCore::HistoryItem* currentItem();
    WebCore::HistoryItem* forwardItem();
    WebCore::HistoryItem* itemAtIndex(int) override;

    void backListWithLimit(int, HistoryItemVector&);
    void forwardListWithLimit(int, HistoryItemVector&);

    int capacity();
    void setCapacity(int);
    bool enabled();
    void setEnabled(bool);
    int backListCount() const final;
    int forwardListCount() const final;
    bool containsItem(WebCore::HistoryItem*);

    void close() override;
    bool closed();

    void removeItem(WebCore::HistoryItem*);
    HistoryItemVector& entries();

private:
    explicit BackForwardList();

    HistoryItemVector m_entries;
    HistoryItemHashSet m_entryHash;
    unsigned m_current;
    unsigned m_capacity;
    bool m_closed;
    bool m_enabled;
};
