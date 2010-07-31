/*
 * Copyright (C) 2006 Apple Computer, Inc.  All rights reserved.
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

#ifndef BackForwardList_h
#define BackForwardList_h

#include <wtf/RefCounted.h>
#include <wtf/PassRefPtr.h>
#include <wtf/Vector.h>

namespace WebCore {

class HistoryItem;

typedef Vector<RefPtr<HistoryItem> > HistoryItemVector;

#if PLATFORM(CHROMIUM)
// In the Chromium port, the back/forward list is managed externally.
// See BackForwardListChromium.cpp
class BackForwardListClient {
public:
    virtual ~BackForwardListClient() {}
    virtual void addItem(PassRefPtr<HistoryItem>) = 0;
    virtual void goToItem(HistoryItem*) = 0;
    virtual HistoryItem* currentItem() = 0;
    virtual HistoryItem* itemAtIndex(int) = 0;
    virtual int backListCount() = 0;
    virtual int forwardListCount() = 0;
    virtual void close() = 0;
};
#endif

class BackForwardList : public RefCounted<BackForwardList> {
public: 
    virtual ~BackForwardList()
    {
    }

    virtual bool isBackForwardListImpl() const { return false; }

#if PLATFORM(CHROMIUM)
    // Must be called before any other methods. 
    virtual void setClient(BackForwardListClient*) = 0;
#endif

    virtual void addItem(PassRefPtr<HistoryItem>) = 0;
    virtual void goBack() = 0;
    virtual void goForward() = 0;
    virtual void goToItem(HistoryItem*) = 0;
        
    virtual HistoryItem* backItem() = 0;
    virtual HistoryItem* currentItem() = 0;
    virtual HistoryItem* forwardItem() = 0;
    virtual HistoryItem* itemAtIndex(int) = 0;

    virtual void backListWithLimit(int, HistoryItemVector&) = 0;
    virtual void forwardListWithLimit(int, HistoryItemVector&) = 0;

    virtual int capacity() = 0;
    virtual void setCapacity(int) = 0;
    virtual bool enabled() = 0;
    virtual void setEnabled(bool) = 0;
    virtual int backListCount() = 0;
    virtual int forwardListCount() = 0;
    virtual bool containsItem(HistoryItem*) = 0;

    virtual void close() = 0;
    virtual bool closed() = 0;
    
    virtual void removeItem(HistoryItem*)  = 0;
    virtual HistoryItemVector& entries()  = 0;
    
#if ENABLE(WML)
    virtual void clearWMLPageHistory()  = 0;
#endif

protected:
    BackForwardList()
    {
    }
};
    
} // namespace WebCore

#endif // BackForwardList_h
