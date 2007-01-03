/*
 * Copyright (C) 2006 Apple Computer, Inc.  All rights reserved.
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

#ifndef BackForwardList_H
#define BackForwardList_H

#include "Shared.h"
#include <wtf/Forward.h>
#include <wtf/Vector.h>

namespace WebCore {

class HistoryItem;
typedef Vector<RefPtr<HistoryItem> > HistoryItemVector;

class BackForwardList : public Shared<BackForwardList> {
public: 
    BackForwardList();
    ~BackForwardList();
    
    void addItem(PassRefPtr<HistoryItem>);
    void goBack();
    void goForward();
    void goToItem(HistoryItem*);
        
    HistoryItem* backItem();
    HistoryItem* currentItem();
    HistoryItem* forwardItem();
    HistoryItem* itemAtIndex(int);

    void backListWithLimit(int, HistoryItemVector&);
    void forwardListWithLimit(int, HistoryItemVector&);

    int capacity();
    void setCapacity(int);
    int backListCount();
    int forwardListCount();
    bool containsItem(HistoryItem*);

    static void setDefaultPageCacheSize(unsigned);
    static unsigned defaultPageCacheSize();
    void setPageCacheSize(unsigned);
    unsigned pageCacheSize();
    bool usesPageCache();
    
    void close();
    bool closed();
    
    void clearPageCache();
    void removeItem(HistoryItem*);
    HistoryItemVector& entries();
    
private:
    HistoryItemVector m_entries;
    unsigned m_current;
    unsigned m_capacity;
    unsigned m_pageCacheSize;
    bool m_closed;
}; //class BackForwardList
    
}; //namespace WebCore

#endif //BACKFORWARDLIST_H
