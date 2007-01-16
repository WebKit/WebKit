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

#ifndef PageCache_h
#define PageCache_h

#include "DocumentLoader.h"
#include "PageState.h"
#include "Shared.h"
#include <wtf/Forward.h>
#include <wtf/RefPtr.h>

#if PLATFORM(MAC)
#include "RetainPtr.h"
typedef struct objc_object* id;
#endif

namespace WebCore {

class PageCache : public Shared<PageCache> {
public:
    PageCache();
    ~PageCache();
    
    void close();
    
    void setPageState(PassRefPtr<PageState>);
    PageState* pageState();
    void setTimeStamp(double);
    void setTimeStampToNow();
    double timeStamp() const;
    void setDocumentLoader(PassRefPtr<DocumentLoader>);
    DocumentLoader* documentLoader();
#if PLATFORM(MAC)
    void setDocumentView(id);
    id documentView();
#endif

private:
    // FIXME: <rdar://problem/4887095>
    // PageCache should consume PageState and take its role, as well.  The reasons for the division are obsolete
    // now that everything is in WebCore instead of split with WebKit
    RefPtr<PageState> m_pageState;
    RefPtr<DocumentLoader> m_documentLoader;
    double m_timeStamp;

#if PLATFORM(MAC)
    RetainPtr<id> m_documentView;
#endif
}; // class PageCache

} // namespace WebCore

#endif // PageCache_h

