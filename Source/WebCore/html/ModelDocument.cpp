/*
 * Copyright (C) 2021 Apple Inc. All Rights Reserved.
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
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "ModelDocument.h"

#if ENABLE(MODEL_ELEMENT)

#include "Document.h"
#include "DocumentLoader.h"
#include "EventNames.h"
#include "FrameLoader.h"
#include "FrameLoaderClient.h"
#include "HTMLBodyElement.h"
#include "HTMLHeadElement.h"
#include "HTMLHtmlElement.h"
#include "HTMLMetaElement.h"
#include "HTMLModelElement.h"
#include "HTMLNames.h"
#include "HTMLSourceElement.h"
#include "HTMLStyleElement.h"
#include "RawDataDocumentParser.h"
#include <wtf/IsoMallocInlines.h>
#include <wtf/text/StringBuilder.h>

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(ModelDocument);

using namespace HTMLNames;

class ModelDocumentParser final : public RawDataDocumentParser {
public:
    static Ref<ModelDocumentParser> create(ModelDocument& document)
    {
        return adoptRef(*new ModelDocumentParser(document));
    }

private:
    ModelDocumentParser(ModelDocument& document)
        : RawDataDocumentParser { document }
        , m_outgoingReferrer { document.outgoingReferrer() }
    {
    }

    void createDocumentStructure();

    void appendBytes(DocumentWriter&, const uint8_t*, size_t) final;
    void finish() final;

    WeakPtr<HTMLModelElement, WeakPtrImplWithEventTargetData> m_modelElement;
    String m_outgoingReferrer;
};

void ModelDocumentParser::createDocumentStructure()
{
    auto& document = *this->document();

    auto rootElement = HTMLHtmlElement::create(document);
    document.appendChild(rootElement);
    document.setCSSTarget(rootElement.ptr());
    rootElement->insertedByParser();

    if (document.frame())
        document.frame()->injectUserScripts(UserScriptInjectionTime::DocumentStart);

    auto headElement = HTMLHeadElement::create(document);
    rootElement->appendChild(headElement);

    auto metaElement = HTMLMetaElement::create(document);
    metaElement->setAttributeWithoutSynchronization(nameAttr, "viewport"_s);
    metaElement->setAttributeWithoutSynchronization(contentAttr, "width=device-width,initial-scale=1"_s);
    headElement->appendChild(metaElement);

    auto styleElement = HTMLStyleElement::create(document);
    auto styleContent = "body { background-color: white; text-align: center; }\n"
        "@media (prefers-color-scheme: dark) { body { background-color: rgb(32, 32, 37); } }\n"
        "model { width: 80vw; height: 80vh; }\n"_s;
    styleElement->setTextContent(styleContent);
    headElement->appendChild(styleElement);

    auto body = HTMLBodyElement::create(document);
    rootElement->appendChild(body);

    auto modelElement = HTMLModelElement::create(HTMLNames::modelTag, document);
    m_modelElement = modelElement.get();
    modelElement->setAttributeWithoutSynchronization(interactiveAttr, emptyAtom());

    auto sourceElement = HTMLSourceElement::create(HTMLNames::sourceTag, document);
    sourceElement->setAttributeWithoutSynchronization(srcAttr, AtomString { document.url().string() });
    if (RefPtr loader = document.loader())
        sourceElement->setAttributeWithoutSynchronization(typeAttr, AtomString { loader->responseMIMEType() });

    modelElement->appendChild(sourceElement);

    body->appendChild(modelElement);
    document.setHasVisuallyNonEmptyCustomContent();

    auto frame = document.frame();
    if (!frame)
        return;

    frame->loader().activeDocumentLoader()->setMainResourceDataBufferingPolicy(DataBufferingPolicy::DoNotBufferData);
    frame->loader().setOutgoingReferrer(document.completeURL(m_outgoingReferrer));
}

void ModelDocumentParser::appendBytes(DocumentWriter&, const uint8_t*, size_t)
{
    if (!m_modelElement)
        createDocumentStructure();
}

void ModelDocumentParser::finish()
{
    document()->finishedParsing();
}

ModelDocument::ModelDocument(Frame* frame, const Settings& settings, const URL& url)
    : HTMLDocument(frame, settings, url, { }, { DocumentClass::Model })
{
    if (frame)
        m_outgoingReferrer = frame->loader().outgoingReferrer();
}

Ref<DocumentParser> ModelDocument::createParser()
{
    return ModelDocumentParser::create(*this);
}

}

#endif
