/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
 * Copyright (C) 2015-2017 Apple Inc. All rights reserved.
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


#include "config.h"
#include "JSCustomElementInterface.h"

#include "DOMWrapperWorld.h"
#include "HTMLUnknownElement.h"
#include "JSDOMBinding.h"
#include "JSDOMConvertNullable.h"
#include "JSDOMConvertStrings.h"
#include "JSDOMWindow.h"
#include "JSElement.h"
#include "JSExecState.h"
#include "JSExecStateInstrumentation.h"
#include "JSHTMLElement.h"
#include "ScriptExecutionContext.h"
#include <JavaScriptCore/JSLock.h>
#include <JavaScriptCore/WeakInlines.h>

namespace WebCore {
using namespace JSC;

JSCustomElementInterface::JSCustomElementInterface(const QualifiedName& name, JSObject* constructor, JSDOMGlobalObject* globalObject)
    : ActiveDOMCallback(globalObject->scriptExecutionContext())
    , m_name(name)
    , m_constructor(constructor)
    , m_isolatedWorld(globalObject->world())
{
}

JSCustomElementInterface::~JSCustomElementInterface() = default;

static RefPtr<Element> constructCustomElementSynchronously(Document&, VM&, JSGlobalObject&, JSObject* constructor, const AtomString& localName);

Ref<Element> JSCustomElementInterface::constructElementWithFallback(Document& document, const AtomString& localName)
{
    if (auto element = tryToConstructCustomElement(document, localName))
        return element.releaseNonNull();

    auto element = HTMLUnknownElement::create(QualifiedName(nullAtom(), localName, HTMLNames::xhtmlNamespaceURI), document);
    element->setIsCustomElementUpgradeCandidate();
    element->setIsFailedCustomElement(*this);

    return element;
}

Ref<Element> JSCustomElementInterface::constructElementWithFallback(Document& document, const QualifiedName& name)
{
    if (auto element = tryToConstructCustomElement(document, name.localName())) {
        if (!name.prefix().isNull())
            element->setPrefix(name.prefix());
        return element.releaseNonNull();
    }

    auto element = HTMLUnknownElement::create(name, document);
    element->setIsCustomElementUpgradeCandidate();
    element->setIsFailedCustomElement(*this);

    return element;
}

RefPtr<Element> JSCustomElementInterface::tryToConstructCustomElement(Document& document, const AtomString& localName)
{
    if (!canInvokeCallback())
        return nullptr;

    Ref<JSCustomElementInterface> protectedThis(*this);

    VM& vm = m_isolatedWorld->vm();
    JSLockHolder lock(vm);
    auto scope = DECLARE_CATCH_SCOPE(vm);

    if (!m_constructor)
        return nullptr;

    ASSERT(&document == scriptExecutionContext());
    auto& lexicalGlobalObject = *document.execState();
    auto element = constructCustomElementSynchronously(document, vm, lexicalGlobalObject, m_constructor.get(), localName);
    EXCEPTION_ASSERT(!!scope.exception() == !element);
    if (!element) {
        auto* exception = scope.exception();
        scope.clearException();
        reportException(&lexicalGlobalObject, exception);
        return nullptr;
    }

    return element;
}

// https://dom.spec.whatwg.org/#concept-create-element
// 6. 1. If the synchronous custom elements flag is set
static RefPtr<Element> constructCustomElementSynchronously(Document& document, VM& vm, JSGlobalObject& lexicalGlobalObject, JSObject* constructor, const AtomString& localName)
{
    auto scope = DECLARE_THROW_SCOPE(vm);
    auto constructData = getConstructData(vm, constructor);
    if (constructData.type == CallData::Type::None) {
        ASSERT_NOT_REACHED();
        return nullptr;
    }

    JSExecState::instrumentFunction(&document, constructData);
    MarkedArgumentBuffer args;
    ASSERT(!args.hasOverflowed());
    JSValue newElement = construct(&lexicalGlobalObject, constructor, constructData, args);
    InspectorInstrumentation::didCallFunction(&document);
    RETURN_IF_EXCEPTION(scope, nullptr);

    ASSERT(!newElement.isEmpty());
    HTMLElement* wrappedElement = JSHTMLElement::toWrapped(vm, newElement);
    if (!wrappedElement) {
        throwTypeError(&lexicalGlobalObject, scope, "The result of constructing a custom element must be a HTMLElement"_s);
        return nullptr;
    }

    if (wrappedElement->hasAttributes()) {
        throwNotSupportedError(lexicalGlobalObject, scope, "A newly constructed custom element must not have attributes"_s);
        return nullptr;
    }
    if (wrappedElement->hasChildNodes()) {
        throwNotSupportedError(lexicalGlobalObject, scope, "A newly constructed custom element must not have child nodes"_s);
        return nullptr;
    }
    if (wrappedElement->parentNode()) {
        throwNotSupportedError(lexicalGlobalObject, scope, "A newly constructed custom element must not have a parent node"_s);
        return nullptr;
    }
    if (&wrappedElement->document() != &document) {
        throwNotSupportedError(lexicalGlobalObject, scope, "A newly constructed custom element belongs to a wrong document"_s);
        return nullptr;
    }
    ASSERT(wrappedElement->namespaceURI() == HTMLNames::xhtmlNamespaceURI);
    if (wrappedElement->localName() != localName) {
        throwNotSupportedError(lexicalGlobalObject, scope, "A newly constructed custom element has incorrect local name"_s);
        return nullptr;
    }

    return wrappedElement;
}

void JSCustomElementInterface::upgradeElement(Element& element)
{
    ASSERT(element.tagQName() == name());
    ASSERT(element.isCustomElementUpgradeCandidate());
    if (!canInvokeCallback())
        return;

    Ref<JSCustomElementInterface> protectedThis(*this);
    VM& vm = m_isolatedWorld->vm();
    JSLockHolder lock(vm);
    auto scope = DECLARE_THROW_SCOPE(vm);

    if (!m_constructor)
        return;

    auto* context = scriptExecutionContext();
    if (!context)
        return;
    auto* globalObject = toJSDOMWindow(downcast<Document>(*context).frame(), m_isolatedWorld);
    if (!globalObject)
        return;
    JSGlobalObject* lexicalGlobalObject = globalObject;

    auto constructData = getConstructData(vm, m_constructor.get());
    if (constructData.type == CallData::Type::None) {
        ASSERT_NOT_REACHED();
        return;
    }

    CustomElementReactionQueue::enqueuePostUpgradeReactions(element);

    m_constructionStack.append(&element);

    MarkedArgumentBuffer args;
    ASSERT(!args.hasOverflowed());
    JSExecState::instrumentFunction(context, constructData);
    JSValue returnedElement = construct(lexicalGlobalObject, m_constructor.get(), constructData, args);
    InspectorInstrumentation::didCallFunction(context);

    m_constructionStack.removeLast();

    if (UNLIKELY(scope.exception())) {
        element.setIsFailedCustomElement(*this);
        reportException(lexicalGlobalObject, scope.exception());
        return;
    }

    Element* wrappedElement = JSElement::toWrapped(vm, returnedElement);
    if (!wrappedElement || wrappedElement != &element) {
        element.setIsFailedCustomElement(*this);
        reportException(lexicalGlobalObject, createDOMException(lexicalGlobalObject, TypeError, "Custom element constructor returned a wrong element"));
        return;
    }
    element.setIsDefinedCustomElement(*this);
}

void JSCustomElementInterface::invokeCallback(Element& element, JSObject* callback, const WTF::Function<void(JSGlobalObject*, JSDOMGlobalObject*, MarkedArgumentBuffer&)>& addArguments)
{
    if (!canInvokeCallback())
        return;

    auto* context = scriptExecutionContext();
    if (!context)
        return;

    Ref<JSCustomElementInterface> protectedThis(*this);
    VM& vm = m_isolatedWorld->vm();
    JSLockHolder lock(vm);

    auto* globalObject = toJSDOMWindow(downcast<Document>(*context).frame(), m_isolatedWorld);
    if (!globalObject)
        return;
    JSGlobalObject* lexicalGlobalObject = globalObject;

    JSObject* jsElement = asObject(toJS(lexicalGlobalObject, globalObject, element));

    auto callData = getCallData(vm, callback);
    ASSERT(callData.type != CallData::Type::None);

    MarkedArgumentBuffer args;
    addArguments(lexicalGlobalObject, globalObject, args);
    RELEASE_ASSERT(!args.hasOverflowed());

    JSExecState::instrumentFunction(context, callData);

    NakedPtr<JSC::Exception> exception;
    JSExecState::call(lexicalGlobalObject, callback, callData, jsElement, args, exception);

    InspectorInstrumentation::didCallFunction(context);

    if (exception)
        reportException(lexicalGlobalObject, exception);
}

void JSCustomElementInterface::setConnectedCallback(JSC::JSObject* callback)
{
    m_connectedCallback = callback;
}

void JSCustomElementInterface::invokeConnectedCallback(Element& element)
{
    invokeCallback(element, m_connectedCallback.get());
}

void JSCustomElementInterface::setDisconnectedCallback(JSC::JSObject* callback)
{
    m_disconnectedCallback = callback;
}

void JSCustomElementInterface::invokeDisconnectedCallback(Element& element)
{
    invokeCallback(element, m_disconnectedCallback.get());
}

void JSCustomElementInterface::setAdoptedCallback(JSC::JSObject* callback)
{
    m_adoptedCallback = callback;
}

void JSCustomElementInterface::invokeAdoptedCallback(Element& element, Document& oldDocument, Document& newDocument)
{
    invokeCallback(element, m_adoptedCallback.get(), [&](JSGlobalObject* lexicalGlobalObject, JSDOMGlobalObject* globalObject, MarkedArgumentBuffer& args) {
        args.append(toJS(lexicalGlobalObject, globalObject, oldDocument));
        args.append(toJS(lexicalGlobalObject, globalObject, newDocument));
    });
}

void JSCustomElementInterface::setAttributeChangedCallback(JSC::JSObject* callback, const Vector<String>& observedAttributes)
{
    m_attributeChangedCallback = callback;
    m_observedAttributes.clear();
    for (auto& name : observedAttributes)
        m_observedAttributes.add(name);
}

void JSCustomElementInterface::invokeAttributeChangedCallback(Element& element, const QualifiedName& attributeName, const AtomString& oldValue, const AtomString& newValue)
{
    invokeCallback(element, m_attributeChangedCallback.get(), [&](JSGlobalObject* lexicalGlobalObject, JSDOMGlobalObject*, MarkedArgumentBuffer& args) {
        args.append(toJS<IDLDOMString>(*lexicalGlobalObject, attributeName.localName()));
        args.append(toJS<IDLNullable<IDLDOMString>>(*lexicalGlobalObject, oldValue));
        args.append(toJS<IDLNullable<IDLDOMString>>(*lexicalGlobalObject, newValue));
        args.append(toJS<IDLNullable<IDLDOMString>>(*lexicalGlobalObject, attributeName.namespaceURI()));
    });
}

void JSCustomElementInterface::didUpgradeLastElementInConstructionStack()
{
    m_constructionStack.last() = nullptr;
}

} // namespace WebCore
