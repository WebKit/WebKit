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
 
#ifndef PageState_h
#define PageState_h

#include "KURL.h"
#include "Shared.h"
#include <wtf/Forward.h>
#include <wtf/OwnPtr.h>

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

    class PageState : public Shared<PageState> {
    public:
        static PassRefPtr<PageState> create(Page*);
        ~PageState();

        void clear();

        Document* document() { return m_document.get(); }
        Node* mousePressNode() { return m_mousePressNode.get(); }
        const KURL& URL() { return m_URL; }

        void restore(Page*);

    private:
        PageState(Page*);

        RefPtr<Document> m_document;
        RefPtr<FrameView> m_view;
        RefPtr<Node> m_mousePressNode;
        KURL m_URL;
        OwnPtr<KJS::SavedProperties> m_windowProperties;
        OwnPtr<KJS::SavedProperties> m_locationProperties;
        OwnPtr<KJS::SavedBuiltins> m_interpreterBuiltins;
        OwnPtr<KJS::PausedTimeouts> m_pausedTimeouts;
    }; // class PageState

} // namespace WebCore

#endif // PageState_h
