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

#include "JSMutationCallback.h"

#include "JSDOMWindowBase.h"
#include "JSMutationObserver.h"
#include "JSMutationRecord.h"
#include "ScriptExecutionContext.h"
#include <runtime/JSLock.h>

using namespace JSC;

namespace WebCore {

bool JSMutationCallback::handleEvent(MutationRecordArray* mutations, MutationObserver* observer)
{
    if (!canInvokeCallback())
        return true;

    RefPtr<JSMutationCallback> protect(this);

    JSLockHolder lock(JSDOMWindowBase::commonJSGlobalData());

    ExecState* exec = m_data->globalObject()->globalExec();

    MarkedArgumentBuffer mutationList;
    for (size_t i = 0; i < mutations->size(); ++i)
        mutationList.append(toJS(exec, m_data->globalObject(), mutations->at(i).get()));

    JSValue jsObserver = toJS(exec, m_data->globalObject(), observer);

    MarkedArgumentBuffer args;
    args.append(constructArray(exec, 0, m_data->globalObject(), mutationList));
    args.append(jsObserver);

    bool raisedException = false;
    m_data->invokeCallback(jsObserver, args, &raisedException);
    return !raisedException;
}

} // namespace WebCore
