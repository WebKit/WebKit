/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
 * Copyright (C) 2015-2016 Apple Inc. All rights reserved.
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

#if ENABLE(CUSTOM_ELEMENTS)

#include "DOMWrapperWorld.h"
#include "JSDOMGlobalObject.h"
#include "JSElement.h"
#include "JSHTMLElement.h"
#include "JSMainThreadExecState.h"
#include "JSMainThreadExecStateInstrumentation.h"
#include "ScriptExecutionContext.h"
#include <heap/WeakInlines.h>
#include <runtime/JSLock.h>

using namespace JSC;

namespace WebCore {

JSCustomElementInterface::JSCustomElementInterface(const QualifiedName& name, JSObject* constructor, JSDOMGlobalObject* globalObject)
    : ActiveDOMCallback(globalObject->scriptExecutionContext())
    , m_name(name)
    , m_constructor(constructor)
    , m_isolatedWorld(&globalObject->world())
{
}

JSCustomElementInterface::~JSCustomElementInterface()
{
}

RefPtr<Element> JSCustomElementInterface::constructElement(const AtomicString& tagName, ShouldClearException shouldClearException)
{
    if (!canInvokeCallback())
        return nullptr;

    Ref<JSCustomElementInterface> protectedThis(*this);

    JSLockHolder lock(m_isolatedWorld->vm());

    if (!m_constructor)
        return nullptr;

    ScriptExecutionContext* context = scriptExecutionContext();
    if (!context)
        return nullptr;
    ASSERT(context->isDocument());
    JSDOMGlobalObject* globalObject = toJSDOMGlobalObject(context, *m_isolatedWorld);
    ExecState* state = globalObject->globalExec();

    ConstructData constructData;
    ConstructType constructType = m_constructor->methodTable()->getConstructData(m_constructor.get(), constructData);
    if (constructType == ConstructType::None) {
        ASSERT_NOT_REACHED();
        return nullptr;
    }

    MarkedArgumentBuffer args;
    args.append(jsStringWithCache(state, tagName));

    InspectorInstrumentationCookie cookie = JSMainThreadExecState::instrumentFunctionConstruct(context, constructType, constructData);
    JSValue newElement = construct(state, m_constructor.get(), constructType, constructData, args);
    InspectorInstrumentation::didCallFunction(cookie, context);

    if (shouldClearException == ShouldClearException::Clear && state->hadException())
        state->clearException();

    if (newElement.isEmpty())
        return nullptr;

    Element* wrappedElement = JSElement::toWrapped(newElement);
    if (!wrappedElement)
        return nullptr;
    wrappedElement->setCustomElementIsResolved(*this);
    return wrappedElement;
}

void JSCustomElementInterface::upgradeElement(Element& element)
{
    ASSERT(element.isUnresolvedCustomElement());
    if (!canInvokeCallback())
        return;

    Ref<JSCustomElementInterface> protectedThis(*this);
    VM& vm = m_isolatedWorld->vm();
    JSLockHolder lock(vm);
    auto scope = DECLARE_THROW_SCOPE(vm);

    if (!m_constructor)
        return;

    ScriptExecutionContext* context = scriptExecutionContext();
    if (!context)
        return;
    ASSERT(context->isDocument());
    JSDOMGlobalObject* globalObject = toJSDOMGlobalObject(context, *m_isolatedWorld);
    ExecState* state = globalObject->globalExec();
    if (state->hadException())
        return;

    ConstructData constructData;
    ConstructType constructType = m_constructor->methodTable()->getConstructData(m_constructor.get(), constructData);
    if (constructType == ConstructType::None) {
        ASSERT_NOT_REACHED();
        return;
    }

    m_constructionStack.append(&element);

    MarkedArgumentBuffer args;
    InspectorInstrumentationCookie cookie = JSMainThreadExecState::instrumentFunctionConstruct(context, constructType, constructData);
    JSValue returnedElement = construct(state, m_constructor.get(), constructType, constructData, args);
    InspectorInstrumentation::didCallFunction(cookie, context);

    m_constructionStack.removeLast();

    if (state->hadException())
        return;

    Element* wrappedElement = JSElement::toWrapped(returnedElement);
    if (!wrappedElement || wrappedElement != &element) {
        throwInvalidStateError(*state, scope, "Custom element constructor failed to upgrade an element");
        return;
    }
    wrappedElement->setCustomElementIsResolved(*this);
}

void JSCustomElementInterface::invokeCallback(Element& element, JSObject* callback, const WTF::Function<void(ExecState*, JSDOMGlobalObject*, MarkedArgumentBuffer&)>& addArguments)
{
    if (!canInvokeCallback())
        return;

    auto* context = scriptExecutionContext();
    if (!context)
        return;

    Ref<JSCustomElementInterface> protectedThis(*this);
    JSLockHolder lock(m_isolatedWorld->vm());

    ASSERT(context);
    ASSERT(context->isDocument());
    JSDOMGlobalObject* globalObject = toJSDOMGlobalObject(context, *m_isolatedWorld);
    ExecState* state = globalObject->globalExec();

    JSObject* jsElement = asObject(toJS(state, globalObject, element));

    CallData callData;
    CallType callType = callback->methodTable()->getCallData(callback, callData);
    ASSERT(callType != CallType::None);

    MarkedArgumentBuffer args;
    addArguments(state, globalObject, args);

    InspectorInstrumentationCookie cookie = JSMainThreadExecState::instrumentFunctionCall(context, callType, callData);

    NakedPtr<Exception> exception;
    JSMainThreadExecState::call(state, callback, callType, callData, jsElement, args, exception);

    InspectorInstrumentation::didCallFunction(cookie, context);

    if (exception)
        reportException(state, exception);
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
    invokeCallback(element, m_adoptedCallback.get(), [&](ExecState* state, JSDOMGlobalObject* globalObject, MarkedArgumentBuffer& args) {
        args.append(toJS(state, globalObject, oldDocument));
        args.append(toJS(state, globalObject, newDocument));
    });
}

void JSCustomElementInterface::setAttributeChangedCallback(JSC::JSObject* callback, const Vector<String>& observedAttributes)
{
    m_attributeChangedCallback = callback;
    m_observedAttributes.clear();
    for (auto& name : observedAttributes)
        m_observedAttributes.add(name);
}

void JSCustomElementInterface::invokeAttributeChangedCallback(Element& element, const QualifiedName& attributeName, const AtomicString& oldValue, const AtomicString& newValue)
{
    invokeCallback(element, m_attributeChangedCallback.get(), [&](ExecState* state, JSDOMGlobalObject*, MarkedArgumentBuffer& args) {
        args.append(jsStringWithCache(state, attributeName.localName()));
        args.append(jsStringOrNull(state, oldValue));
        args.append(jsStringOrNull(state, newValue));
        args.append(jsStringOrNull(state, attributeName.namespaceURI()));
    });
}

void JSCustomElementInterface::didUpgradeLastElementInConstructionStack()
{
    m_constructionStack.last() = nullptr;
}

} // namespace WebCore

#endif
