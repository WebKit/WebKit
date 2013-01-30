/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
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
#include "JSMutationCallback.h"

#include "JSMutationObserver.h"
#include "JSMutationRecord.h"
#include "ScriptExecutionContext.h"
#include <runtime/JSLock.h>

using namespace JSC;

namespace WebCore {

JSMutationCallback::JSMutationCallback(JSObject* callback, JSDOMGlobalObject* globalObject)
    : ActiveDOMCallback(globalObject->scriptExecutionContext())
    , m_callback(PassWeak<JSObject>(callback))
    , m_isolatedWorld(globalObject->world())
{
}

JSMutationCallback::~JSMutationCallback()
{
}

bool JSMutationCallback::handleEvent(MutationRecordArray* mutations, MutationObserver* observer)
{
    if (!canInvokeCallback())
        return true;

    RefPtr<JSMutationCallback> protect(this);

    JSLockHolder lock(m_isolatedWorld->globalData());

    if (!m_callback)
        return false;

    ScriptExecutionContext* context = scriptExecutionContext();
    if (!context)
        return false;

    JSDOMGlobalObject* globalObject = toJSDOMGlobalObject(context, m_isolatedWorld.get());
    ExecState* exec = globalObject->globalExec();

    MarkedArgumentBuffer mutationList;
    for (size_t i = 0; i < mutations->size(); ++i)
        mutationList.append(toJS(exec, globalObject, mutations->at(i).get()));

    JSValue jsObserver = toJS(exec, globalObject, observer);

    MarkedArgumentBuffer args;
    args.append(constructArray(exec, 0, globalObject, mutationList));
    args.append(jsObserver);

    bool raisedException = false;
    // FIXME: Extract invokeCallback() method.
    JSCallbackData(m_callback.get(), globalObject).invokeCallback(jsObserver, args, &raisedException);
    return !raisedException;
}

} // namespace WebCore
