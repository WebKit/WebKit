/*
 * Copyright (C) 2006, 2007, 2008 Apple Inc. All rights reserved.
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

#ifndef CachedPage_h
#define CachedPage_h

#include "KURL.h"
#include <wtf/OwnPtr.h>

namespace KJS {    
    class SavedBuiltins;
    struct SavedProperties;
}

namespace WebCore {
    
    class CachedPagePlatformData;
    class Document;
    class DocumentLoader;
    class FrameView;
    class Node;
    class Page;
    class PausedTimeouts;

class CachedPage : public RefCounted<CachedPage> {
public:
    static PassRefPtr<CachedPage> create(Page*);
    ~CachedPage();
    
    void clear();
    Document* document() const { return m_document.get(); }
    FrameView* view() const { return m_view.get(); }
    Node* mousePressNode() const { return m_mousePressNode.get(); }
    const KURL& url() const { return m_URL; }
    void restore(Page*);
        
    void setTimeStamp(double);
    void setTimeStampToNow();
    double timeStamp() const;
    void setDocumentLoader(PassRefPtr<DocumentLoader>);
    DocumentLoader* documentLoader();

    void setCachedPagePlatformData(CachedPagePlatformData*);
    CachedPagePlatformData* cachedPagePlatformData();

private:
    CachedPage(Page*);
    RefPtr<DocumentLoader> m_documentLoader;
    double m_timeStamp;

    RefPtr<Document> m_document;
    RefPtr<FrameView> m_view;
    RefPtr<Node> m_mousePressNode;
    KURL m_URL;
    OwnPtr<KJS::SavedProperties> m_windowProperties;
    OwnPtr<KJS::SavedProperties> m_locationProperties;
    OwnPtr<KJS::SavedProperties> m_windowLocalStorage;
    OwnPtr<KJS::SavedBuiltins> m_windowBuiltins;
    OwnPtr<PausedTimeouts> m_pausedTimeouts;
    OwnPtr<CachedPagePlatformData> m_cachedPagePlatformData;
};

} // namespace WebCore

#endif // CachedPage_h

