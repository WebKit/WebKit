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

#include "ActiveDOMObject.h"
#include "ScriptExecutionContext.h"
#include <wtf/Threading.h>

#if ENABLE(INDEXED_DATABASE)

namespace WebCore {

class IDBActiveDOMObject : public ActiveDOMObject {
public:
    ThreadIdentifier originThreadID() const { return m_originThreadID; }

    void contextDestroyed() final {
        ASSERT(currentThread() == m_originThreadID);

        Locker<Lock> lock(m_scriptExecutionContextLock);
        ActiveDOMObject::contextDestroyed();
    }

    template<typename T, typename... Parameters, typename... Arguments>
    void performCallbackOnOriginThread(T& object, void (T::*method)(Parameters...), Arguments&&... arguments)
    {
        ASSERT(originThreadID() == object.originThreadID());

        if (object.originThreadID() == currentThread()) {
            (object.*method)(arguments...);
            return;
        }

        Locker<Lock> lock(m_scriptExecutionContextLock);

        ScriptExecutionContext* context = scriptExecutionContext();
        if (!context)
            return;

        context->postCrossThreadTask(object, method, arguments...);
    }

    void callFunctionOnOriginThread(WTF::Function<void ()>&& function)
    {
        if (originThreadID() == currentThread()) {
            function();
            return;
        }

        Locker<Lock> lock(m_scriptExecutionContextLock);

        ScriptExecutionContext* context = scriptExecutionContext();
        if (!context)
            return;

        context->postTask(WTFMove(function));
    }

protected:
    IDBActiveDOMObject(ScriptExecutionContext* context)
        : ActiveDOMObject(context)
    {
        ASSERT(context);
    }

private:
    ThreadIdentifier m_originThreadID { currentThread() };
    Lock m_scriptExecutionContextLock;
};

} // namespace WebCore

#endif // ENABLE(INDEXED_DATABASE)
