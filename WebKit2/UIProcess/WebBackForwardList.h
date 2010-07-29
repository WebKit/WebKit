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

#ifndef WebBackForwardList_h
#define WebBackForwardList_h

#include "APIObject.h"
#include "ImmutableArray.h"
#include "WebBackForwardListItem.h"
#include <wtf/PassRefPtr.h>
#include <wtf/RefPtr.h>
#include <wtf/Vector.h>

namespace WebKit {

class WebPageProxy;

typedef Vector<RefPtr<WebBackForwardListItem> > BackForwardListItemVector;

/*
 *          Current
 *   |---------*--------------| Entries
 *      Back        Forward
 */

class WebBackForwardList : public APIObject {
public:
    static PassRefPtr<WebBackForwardList> create(WebPageProxy* page)
    {
        return adoptRef(new WebBackForwardList(page));
    }
    ~WebBackForwardList();

    void addItem(WebBackForwardListItem*);
    void goToItem(WebBackForwardListItem*);

    WebBackForwardListItem* currentItem();
    WebBackForwardListItem* backItem();
    WebBackForwardListItem* forwardItem();
    WebBackForwardListItem* itemAtIndex(int);

    int backListCount();
    int forwardListCount();

    BackForwardListItemVector backListWithLimit(unsigned limit);
    BackForwardListItemVector forwardListWithLimit(unsigned limit);

    PassRefPtr<ImmutableArray> backListAsImmutableArrayWithLimit(unsigned limit);
    PassRefPtr<ImmutableArray> forwardListAsImmutableArrayWithLimit(unsigned limit);

private:
    WebBackForwardList(WebPageProxy*);

    virtual Type type() const { return TypeBackForwardList; }

    WebPageProxy* m_page;
    BackForwardListItemVector m_entries;
    unsigned m_current;
    unsigned m_capacity;
    bool m_closed;
    bool m_enabled;
};

} // namespace WebKit

#endif // WebBackForwardList_h
