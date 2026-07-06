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

#include "config.h"
#include "PDFJSDocument.h"

#if ENABLE(PDFJS)

#include "DocumentLoader.h"
#include "DocumentSettingsValues.h"
#include "EventListener.h"
#include "EventNames.h"
#include "FrameDestructionObserverInlines.h"
#include "HTMLAnchorElement.h"
#include "HTMLBodyElement.h"
#include "HTMLHeadElement.h"
#include "HTMLHtmlElement.h"
#include "HTMLIFrameElement.h"
#include "HTMLLinkElement.h"
#include "HTMLNames.h"
#include "HTMLScriptElement.h"
#include "LocalDOMWindow.h"
#include "LocalFrame.h"
#include "RawDataDocumentParser.h"
#include "ScriptController.h"
#include "Settings.h"
#include "UserScriptTypes.h"
#include "WindowPostMessageOptions.h"
#include <JavaScriptCore/JSArrayBuffer.h>
#include <JavaScriptCore/JSGlobalObjectInlines.h>
#include <JavaScriptCore/JSString.h>
#include <JavaScriptCore/ObjectConstructor.h>
#include <wtf/TZoneMallocInlines.h>

namespace WebCore {

WTF_MAKE_TZONE_ALLOCATED_IMPL(PDFJSDocument);

using namespace HTMLNames;

/* PDFJSDocumentParser: this receives the PDF bytes */

class PDFJSDocumentParser final : public RawDataDocumentParser {
public:
    static Ref<PDFJSDocumentParser> create(PDFJSDocument& document)
    {
        return adoptRef(*new PDFJSDocumentParser(document));
    }

private:
    explicit PDFJSDocumentParser(PDFJSDocument& document)
        : RawDataDocumentParser(document)
    {
    }

    PDFJSDocument& document() const;

    void appendBytes(DocumentWriter&, std::span<const uint8_t>) override;
    void finish() override;
};

inline PDFJSDocument& PDFJSDocumentParser::document() const
{
    // Only used during parsing, so document is guaranteed to be non-null.
    ASSERT(RawDataDocumentParser::document());
    return downcast<PDFJSDocument>(*RawDataDocumentParser::document());
}

void PDFJSDocumentParser::appendBytes(DocumentWriter&, std::span<const uint8_t>)
{
    document().updateDuringParsing();
}

void PDFJSDocumentParser::finish()
{
    document().finishedParsing();
}

/* PDFJSDocumentEventListener: event listener for the PDFJSDocument iframe */

class PDFJSDocumentEventListener final : public EventListener {
public:
    static Ref<PDFJSDocumentEventListener> create(PDFJSDocument& document) { return adoptRef(*new PDFJSDocumentEventListener(document)); }

private:
    explicit PDFJSDocumentEventListener(PDFJSDocument& document)
        : EventListener(PDFJSDocumentEventListenerType)
        , m_document(document)
    {
    }

    bool operator==(const EventListener&) const override;
    void handleEvent(ScriptExecutionContext&, Event&) override;

    WeakPtr<PDFJSDocument, WeakPtrImplWithEventTargetData> m_document;
};

void PDFJSDocumentEventListener::handleEvent(ScriptExecutionContext&, Event& event)
{
    if (is<HTMLIFrameElement>(event.target()) && event.type() == eventNames().loadEvent) {
        m_document->injectStyleAndContentScript();
    } else if (is<HTMLScriptElement>(event.target()) && event.type() == eventNames().loadEvent) {
        m_document->setContentScriptLoaded(true);
        if (m_document->isFinishedParsing())
            m_document->finishLoadingPDF();
    } else
        ASSERT_NOT_REACHED();
}

bool PDFJSDocumentEventListener::operator==(const EventListener& other) const
{
    // All PDFJSDocumentEventListenerType objects compare as equal; OK since there is only one per document.
    return other.type() == PDFJSDocumentEventListenerType;
}

/* PDFJSDocument */

PDFJSDocument::PDFJSDocument(LocalFrame& frame, const URL& url)
    : HTMLDocument(&frame, frame.settings(), url, { }, { DocumentClass::PDFJS })
{
}

PDFJSDocument::~PDFJSDocument() = default;

Ref<DocumentParser> PDFJSDocument::createParser()
{
    return PDFJSDocumentParser::create(*this);
}

void PDFJSDocument::createDocumentStructure()
{
    // Description of parameters:
    // - Empty `?file=` parameter prevents default pdf from loading.
    auto viewerURL = "webkit-pdfjs-viewer://pdfjs/web/viewer.html?file="_s;
    Ref rootElement = HTMLHtmlElement::create(*this);
    appendChild(rootElement);

    frame()->injectUserScripts(UserScriptInjectionTime::DocumentStart);

    Ref body = HTMLBodyElement::create(*this);
    body->setAttribute(styleAttr, "margin: 0px;height: 100vh;"_s);
    rootElement->appendChild(body);

    m_iframe = HTMLIFrameElement::create(HTMLNames::iframeTag, *this);
    m_iframe->setAttribute(srcAttr, AtomString(viewerURL));
    m_iframe->setAttribute(styleAttr, "width: 100%; height: 100%; border: 0; display: block;"_s);

    m_listener = PDFJSDocumentEventListener::create(*this);
    m_iframe->addEventListener(eventNames().loadEvent, *m_listener);

    body->appendChild(*m_iframe);
}

void PDFJSDocument::updateDuringParsing()
{
    if (!m_iframe)
        createDocumentStructure();
}

void PDFJSDocument::finishedParsing()
{
    ASSERT(m_iframe);
    m_isFinishedParsing = true;
    if (m_isContentScriptLoaded)
        finishLoadingPDF();
}

void PDFJSDocument::postMessageToIframe(const String& name, JSC::JSObject* data)
{
    auto globalObject = this->globalObject();
    auto& vm = globalObject->vm();
    JSC::JSLockHolder lock(vm);

    JSC::JSObject* message = JSC::constructEmptyObject(globalObject);
    message->putDirect(vm, vm.propertyNames->message, JSC::jsNontrivialString(vm, name));
    if (data)
        message->putDirect(vm, JSC::Identifier::fromString(vm, "data"_s), data);

    auto* contentFrame = dynamicDowncast<LocalFrame>(m_iframe->contentFrame());
    if (!contentFrame)
        return;
    auto* contentWindow = contentFrame->window();
    auto* contentWindowGlobalObject = m_iframe->contentDocument()->globalObject();

    WindowPostMessageOptions options;
    if (data)
        options.transfer = Vector { JSC::Strong<JSC::JSObject> { vm, data } };
    auto returnValue = contentWindow->postMessage(*contentWindowGlobalObject, *contentWindow, message, WTF::move(options));
    if (returnValue.hasException())
        returnValue.releaseException();
}

void PDFJSDocument::sendPDFArrayBuffer()
{
    auto* documentLoader = loader();
    ASSERT(documentLoader);
    if (auto mainResourceData = documentLoader->mainResourceData()) {
        if (auto arrayBuffer = mainResourceData->tryCreateArrayBuffer()) {
            auto& vm = globalObject()->vm();
            JSC::JSLockHolder lock(vm);
            auto* dataObject = JSC::JSArrayBuffer::create(vm, globalObject()->arrayBufferStructure(arrayBuffer->sharingMode()), WTF::move(arrayBuffer));
            postMessageToIframe("open-pdf"_s, dataObject);
        }
    }
}

void PDFJSDocument::finishLoadingPDF()
{
    sendPDFArrayBuffer();

    if (m_script) {
        m_script->removeEventListener(eventNames().loadEvent, *m_listener, { });
        m_script = nullptr;
    }

    m_listener = nullptr;
}

void PDFJSDocument::injectStyleAndContentScript()
{
    if (m_injectedStyleAndScript)
        return;

    auto* contentDocument = m_iframe->contentDocument();
    ASSERT(contentDocument->head());
    Ref link = HTMLLinkElement::create(HTMLNames::linkTag, *contentDocument, false);
    link->setAttribute(relAttr, "stylesheet"_s);
#if PLATFORM(COCOA)
    link->setAttribute(hrefAttr, "webkit-pdfjs-viewer://pdfjs/extras/cocoa/style.css"_s);
#elif PLATFORM(GTK) || PLATFORM(WPE)
    link->setAttribute(hrefAttr, "webkit-pdfjs-viewer://pdfjs/extras/adwaita/style.css"_s);
#endif
    contentDocument->head()->appendChild(link);

    ASSERT(contentDocument->body());
    m_script = HTMLScriptElement::create(scriptTag, *contentDocument, false);
    ASSERT(m_listener);
    m_script->addEventListener(eventNames().loadEvent, *m_listener);
    m_script->setAttribute(srcAttr, "webkit-pdfjs-viewer://pdfjs/extras/content-script.js"_s);
    contentDocument->body()->appendChild(*m_script);

    m_injectedStyleAndScript = true;
}

} // namepsace WebCore

#endif // ENABLE(PDFJS)
