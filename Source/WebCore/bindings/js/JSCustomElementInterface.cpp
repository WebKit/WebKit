/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
 * Copyright (C) 2015 Apple Inc. All rights reserved.
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

JSCustomElementInterface::JSCustomElementInterface(JSObject* constructor, JSDOMGlobalObject* globalObject)
    : ActiveDOMCallback(globalObject->scriptExecutionContext())
    , m_constructor(constructor)
    , m_isolatedWorld(&globalObject->world())
{
}

JSCustomElementInterface::~JSCustomElementInterface()
{
}

RefPtr<Element> JSCustomElementInterface::constructElement(const AtomicString& tagName)
{
    if (!canInvokeCallback())
        return nullptr;

    Ref<JSCustomElementInterface> protect(*this);

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
    if (constructType == ConstructTypeNone) {
        ASSERT_NOT_REACHED();
        return nullptr;
    }

    MarkedArgumentBuffer args;
    args.append(jsStringWithCache(state, tagName));

    InspectorInstrumentationCookie cookie = JSMainThreadExecState::instrumentFunctionConstruct(context, constructType, constructData);
    JSValue newElement = construct(state, m_constructor.get(), constructType, constructData, args);
    InspectorInstrumentation::didCallFunction(cookie, context);

    if (newElement.isEmpty())
        return nullptr;

    Element* wrappedElement = JSElement::toWrapped(newElement);
    if (!wrappedElement)
        return nullptr;
    wrappedElement->setIsCustomElement();
    return wrappedElement;
}

} // namespace WebCore

#endif
