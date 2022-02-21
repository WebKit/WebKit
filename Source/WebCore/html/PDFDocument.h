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

#include "HTMLDocument.h"

namespace WebCore {

class HTMLIFrameElement;
class PDFDocumentEventListener;

class PDFDocument final : public HTMLDocument {
    WTF_MAKE_ISO_ALLOCATED(PDFDocument);
public:
    static Ref<PDFDocument> create(Frame& frame, const URL& url)
    {
        return adoptRef(*new PDFDocument(frame, url));
    }

    void updateDuringParsing();
    void finishedParsing();
    void injectContentScript();

    void sendPDFArrayBuffer();
    bool isFinishedParsing() const { return m_isFinishedParsing; }
    void setContentScriptLoaded(bool loaded) { m_isContentScriptLoaded = loaded; }

private:
    PDFDocument(Frame&, const URL&);

    Ref<DocumentParser> createParser() override;

    void createDocumentStructure();
    bool m_isFinishedParsing { false };
    bool m_isContentScriptLoaded { false };
    RefPtr<HTMLIFrameElement> m_iframe;
    RefPtr<PDFDocumentEventListener> m_listener;
};

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_BEGIN(WebCore::PDFDocument)
    static bool isType(const WebCore::Document& document) { return document.isPDFDocument(); }
    static bool isType(const WebCore::Node& node) { return is<WebCore::Document>(node) && isType(downcast<WebCore::Document>(node)); }
SPECIALIZE_TYPE_TRAITS_END()
