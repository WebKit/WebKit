/*
 * Copyright (C) 2016 Apple Inc. All rights reserved.
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

#pragma once

#include "Document.h"

namespace WebCore {

class XMLDocument : public Document {
    WTF_MAKE_ISO_ALLOCATED(XMLDocument);
public:
    static Ref<XMLDocument> create(LocalFrame* frame, const Settings& settings, const URL& url)
    {
        auto document = adoptRef(*new XMLDocument(frame, settings, url, { DocumentClass::XML }));
        document->addToContextsMap();
        return document;
    }

    static Ref<XMLDocument> createXHTML(LocalFrame* frame, const Settings& settings, const URL& url)
    {
        auto document = adoptRef(*new XMLDocument(frame, settings, url, { DocumentClass::XML, DocumentClass::XHTML }));
        document->addToContextsMap();
        return document;
    }

protected:
    XMLDocument(LocalFrame* frame, const Settings& settings, const URL& url, DocumentClasses documentClasses = { })
        : Document(frame, settings, url, documentClasses | DocumentClasses(DocumentClass::XML))
    {
    }
};

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_BEGIN(WebCore::XMLDocument)
    static bool isType(const WebCore::Document& document) { return document.isXMLDocument(); }
    static bool isType(const WebCore::Node& node) { return is<WebCore::Document>(node) && isType(downcast<WebCore::Document>(node)); }
SPECIALIZE_TYPE_TRAITS_END()
