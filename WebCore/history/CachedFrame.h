/*
 * Copyright (C) 2009 Apple Inc. All rights reserved.
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
 
#ifndef CachedFrame_h
#define CachedFrame_h

#include "KURL.h"
#include "ScriptCachedFrameData.h"
#include <wtf/RefPtr.h>

namespace WebCore {
    
    class CachedFrame;
    class CachedFramePlatformData;
    class DOMWindow;
    class Document;
    class DocumentLoader;
    class Frame;
    class FrameView;
    class Node;

typedef Vector<RefPtr<CachedFrame> > CachedFrameVector;

class CachedFrame : public RefCounted<CachedFrame> {
public:
    static PassRefPtr<CachedFrame> create(Frame* frame) { return adoptRef(new CachedFrame(frame)); }
    ~CachedFrame();

    void restore();
    void clear();

    Document* document() const { return m_document.get(); }
    DocumentLoader* documentLoader() const { return m_documentLoader.get(); }
    FrameView* view() const { return m_view.get(); }
    Node* mousePressNode() const { return m_mousePressNode.get(); }
    const KURL& url() const { return m_url; }
    DOMWindow* domWindow() const { return m_cachedFrameScriptData->domWindow(); }

    void setCachedFramePlatformData(CachedFramePlatformData*);
    CachedFramePlatformData* cachedFramePlatformData();

private:
    CachedFrame(Frame*);

    RefPtr<Document> m_document;
    RefPtr<DocumentLoader> m_documentLoader;
    RefPtr<FrameView> m_view;
    RefPtr<Node> m_mousePressNode;
    KURL m_url;
    OwnPtr<ScriptCachedFrameData> m_cachedFrameScriptData;
    OwnPtr<CachedFramePlatformData> m_cachedFramePlatformData;
    
    CachedFrameVector m_childFrames;
};

} // namespace WebCore

#endif // CachedFrame_h
