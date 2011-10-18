/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"

#if ENABLE(MUTATION_OBSERVERS)

#include "JSWebKitMutationObserver.h"

#include "ExceptionCode.h"
#include "JSDictionary.h"
#include "JSMutationCallback.h"
#include "JSNode.h"
#include "Node.h"
#include "WebKitMutationObserver.h"
#include <runtime/Error.h>

using namespace JSC;

namespace WebCore {

EncodedJSValue JSC_HOST_CALL JSWebKitMutationObserverConstructor::constructJSWebKitMutationObserver(ExecState* exec)
{
    if (exec->argumentCount() < 1)
        return throwVMError(exec, createTypeError(exec, "Not enough arguments"));

    JSObject* object = exec->argument(0).getObject();
    if (!object) {
        setDOMException(exec, TYPE_MISMATCH_ERR);
        return JSValue::encode(jsUndefined());
    }

    JSWebKitMutationObserverConstructor* jsConstructor = static_cast<JSWebKitMutationObserverConstructor*>(exec->callee());
    RefPtr<MutationCallback> callback = JSMutationCallback::create(object, jsConstructor->globalObject());
    return JSValue::encode(asObject(toJS(exec, jsConstructor->globalObject(), WebKitMutationObserver::create(callback.release()))));
}

JSValue JSWebKitMutationObserver::observe(ExecState* exec)
{
    if (exec->argumentCount() < 2)
        return throwError(exec, createTypeError(exec, "Not enough arguments"));
    Node* target = toNode(exec->argument(0));
    if (exec->hadException())
        return jsUndefined();

    JSObject* optionsObject = exec->argument(1).getObject();
    if (!optionsObject) {
        setDOMException(exec, TYPE_MISMATCH_ERR);
        return jsUndefined();
    }

    JSDictionary dictionary(exec, optionsObject);
    MutationObserverOptions options = 0;
    bool option;
    if (dictionary.tryGetProperty("childList", option) && option)
        options |= WebKitMutationObserver::ChildList;
    if (dictionary.tryGetProperty("attributes", option) && option)
        options |= WebKitMutationObserver::Attributes;
    if (dictionary.tryGetProperty("subtree", option) && option)
        options |= WebKitMutationObserver::Subtree;
    if (dictionary.tryGetProperty("attributeOldValue", option) && option)
        options |= WebKitMutationObserver::AttributeOldValue;
    if (dictionary.tryGetProperty("characterDataOldValue", option) && option)
        options |= WebKitMutationObserver::CharacterDataOldValue;

    if (exec->hadException())
        return jsUndefined();

    impl()->observe(target, options);
    return jsUndefined();
}

} // namespace WebCore

#endif // ENABLE(MUTATION_OBSERVERS)
