/*
 * Copyright (C) 2013-2017 Apple Inc. All rights reserved.
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
#include "JSDOMGlobalObjectTask.h"

#include "ActiveDOMCallback.h"
#include "JSDOMGlobalObject.h"
#include "JSExecState.h"
#include <JavaScriptCore/Microtask.h>
#include <JavaScriptCore/StrongInlines.h>
#include <wtf/Ref.h>

namespace WebCore {
using namespace JSC;

class JSGlobalObjectCallback final : public RefCounted<JSGlobalObjectCallback>, private ActiveDOMCallback {
public:
    static Ref<JSGlobalObjectCallback> create(JSDOMGlobalObject& globalObject, Ref<JSC::Microtask>&& task)
    {
        return adoptRef(*new JSGlobalObjectCallback(globalObject, WTFMove(task)));
    }

    void call()
    {
        if (!canInvokeCallback())
            return;

        Ref<JSGlobalObjectCallback> protectedThis(*this);
        VM& vm = m_globalObject->vm();
        JSLockHolder lock(vm);
        auto scope = DECLARE_THROW_SCOPE(vm);

        JSGlobalObject* lexicalGlobalObject = m_globalObject.get();

        ScriptExecutionContext* context = m_globalObject->scriptExecutionContext();
        // We will fail to get the context if the frame has been detached.
        if (!context)
            return;
        JSExecState::runTask(lexicalGlobalObject, m_task);
        scope.assertNoException();
    }

private:
    JSGlobalObjectCallback(JSDOMGlobalObject& globalObject, Ref<JSC::Microtask>&& task)
        : ActiveDOMCallback { globalObject.scriptExecutionContext() }
        , m_globalObject { globalObject.vm(), &globalObject }
        , m_task { WTFMove(task) }
    {
    }

    Strong<JSDOMGlobalObject> m_globalObject;
    Ref<JSC::Microtask> m_task;
};

JSGlobalObjectTask::JSGlobalObjectTask(JSDOMGlobalObject& globalObject, Ref<JSC::Microtask>&& task)
    : ScriptExecutionContext::Task({ })
{
    auto callback = JSGlobalObjectCallback::create(globalObject, WTFMove(task));
    m_task = [callback = WTFMove(callback)] (ScriptExecutionContext&) {
        callback->call();
    };
}

} // namespace WebCore
