/*
 * Copyright (C) 2022 Apple Inc. All rights reserved.
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
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#if ENABLE(PDFJS)

#include "HTMLDocument.h"
#include "HTMLScriptElement.h"

namespace WebCore {

class HTMLIFrameElement;
class PDFDocumentEventListener;

class PDFDocument final : public HTMLDocument {
    WTF_MAKE_ISO_ALLOCATED(PDFDocument);
public:
    static Ref<PDFDocument> create(LocalFrame& frame, const URL& url)
    {
        auto document = adoptRef(*new PDFDocument(frame, url));
        document->addToContextsMap();
        return document;
    }

    void updateDuringParsing();
    void finishedParsing();
    void injectStyleAndContentScript();

    void postMessageToIframe(const String& name, JSC::JSObject* data);
    void finishLoadingPDF();

    bool isFinishedParsing() const { return m_isFinishedParsing; }
    void setContentScriptLoaded(bool loaded) { m_isContentScriptLoaded = loaded; }

private:
    PDFDocument(LocalFrame&, const URL&);

    Ref<DocumentParser> createParser() override;

    void createDocumentStructure();
    void sendPDFArrayBuffer();
    bool m_injectedStyleAndScript { false };
    bool m_isFinishedParsing { false };
    bool m_isContentScriptLoaded { false };
    RefPtr<HTMLIFrameElement> m_iframe;
    RefPtr<HTMLScriptElement> m_script;
    RefPtr<PDFDocumentEventListener> m_listener;
};

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_BEGIN(WebCore::PDFDocument)
    static bool isType(const WebCore::Document& document) { return document.isPDFDocument(); }
    static bool isType(const WebCore::Node& node)
    {
        auto* document = dynamicDowncast<WebCore::Document>(node);
        return document && isType(*document);
    }
SPECIALIZE_TYPE_TRAITS_END()

#endif // ENABLE(PDFJS)
