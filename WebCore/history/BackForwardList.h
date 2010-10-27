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

#ifndef BackForwardList_h
#define BackForwardList_h

#include <wtf/RefCounted.h>
#include <wtf/PassRefPtr.h>
#include <wtf/Vector.h>

namespace WebCore {

class HistoryItem;

// FIXME: Remove this and rely on the typedef in BackForwardListImpl
// instead, after removing the virtual functions at the bottom
// of this class.
typedef Vector<RefPtr<HistoryItem> > HistoryItemVector;

// FIXME: Move this class out of this file and into BackForwardListImpl.
// FIXME: Consider replacing this BackForwardListClient concept with a
// function that creates a BackForwardList object. The functions in
// BackForwardList are now almost identical to this, and there is no
// need for the extra level of indirection.
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

// FIXME: Rename this class to BackForwardClient, and rename the
// getter in Page accordingly.
class BackForwardList : public RefCounted<BackForwardList> {
public: 
    virtual ~BackForwardList()
    {
    }

    // FIXME: Move this function to BackForwardListImpl, or eliminate
    // it (see comment at definition of BackForwardListClient class).
#if PLATFORM(CHROMIUM)
    // Must be called before any other methods. 
    virtual void setClient(BackForwardListClient*) = 0;
#endif

    virtual void addItem(PassRefPtr<HistoryItem>) = 0;

    virtual void goToItem(HistoryItem*) = 0;
        
    virtual HistoryItem* itemAtIndex(int) = 0;
    virtual int backListCount() = 0;
    virtual int forwardListCount() = 0;

    virtual bool isActive() = 0;

    virtual void close() = 0;

    // FIXME: Rename this to just "clear" and change it so it's not
    // WML-specific. This is the same operation as clearBackForwardList
    // in the layout test controller; it would be reasonable to have it
    // here even though HTML DOM interfaces don't require it.
#if ENABLE(WML)
    virtual void clearWMLPageHistory()  = 0;
#endif

    HistoryItem* backItem() { return itemAtIndex(-1); }
    HistoryItem* currentItem() { return itemAtIndex(0); }
    HistoryItem* forwardItem() { return itemAtIndex(1); }

    // FIXME: Remove these functions once all call sites are calling them
    // directly on BackForwardListImpl instead of on BackForwardList.
    // There is no need for any of these to be virtual functions and no
    // need to implement them in classes other than BackForwardListImpl.
    // Also remove the HistoryItemVector typedef in this file once this is done.
    virtual void goBack() { }
    virtual void goForward() { }
    virtual void backListWithLimit(int, HistoryItemVector&) { }
    virtual void forwardListWithLimit(int, HistoryItemVector&) { }
    virtual int capacity() { return 0; }
    virtual void setCapacity(int) { }
    virtual bool enabled() { return false; }
    virtual void setEnabled(bool) { }
    virtual bool containsItem(HistoryItem*) { return false; }
    virtual bool closed() { return false; }
    virtual void removeItem(HistoryItem*) { }
    virtual HistoryItemVector& entries() { HistoryItemVector* bogus = 0; return *bogus; }

protected:
    BackForwardList()
    {
    }
};
    
} // namespace WebCore

#endif // BackForwardList_h
