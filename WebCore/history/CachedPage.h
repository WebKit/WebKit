/*
 * Copyright (C) 2006, 2007 Apple Inc.  All rights reserved.
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

#include "DocumentLoader.h"
#include "Shared.h"
#include <wtf/Forward.h>
#include <wtf/RefPtr.h>
#include <wtf/OwnPtr.h>

#if PLATFORM(MAC)
#include <wtf/RetainPtr.h>
typedef struct objc_object* id;
#endif

namespace KJS {
    
    class PausedTimeouts;
    class SavedBuiltins;
    class SavedProperties;
}

namespace WebCore {

    class Document;
    class FrameView;
    class Node;
    class Page;

class CachedPage : public Shared<CachedPage> {
public:
    static PassRefPtr<CachedPage> create(Page*);
    ~CachedPage();
    
    void clear();
    Document* document() const { return m_document.get(); }
    FrameView* view() const { return m_view.get(); }
    Node* mousePressNode() const { return m_mousePressNode.get(); }
    const KURL& URL() const { return m_URL; }
    void restore(Page*);
    
    void close();
    
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
    CachedPage(Page*);
    RefPtr<DocumentLoader> m_documentLoader;
    double m_timeStamp;

    RefPtr<Document> m_document;
    RefPtr<FrameView> m_view;
    RefPtr<Node> m_mousePressNode;
    KURL m_URL;
    OwnPtr<KJS::SavedProperties> m_windowProperties;
    OwnPtr<KJS::SavedProperties> m_locationProperties;
    OwnPtr<KJS::SavedBuiltins> m_interpreterBuiltins;
    OwnPtr<KJS::PausedTimeouts> m_pausedTimeouts;
        
#if PLATFORM(MAC)
    RetainPtr<id> m_documentView;
#endif
}; // class CachedPage

} // namespace WebCore

#endif // CachedPage_h

