/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include "ScriptExecutionContext.h"
#include <wtf/Lock.h>

namespace WebCore {

// A helper class to safely dereference the callback objects held by
// SQLStatement and SQLTransaction on the proper thread. The 'wrapped'
// callback is dereferenced:
// - by destructing the enclosing wrapper - on any thread
// - by calling clear() - on any thread
// - by unwrapping and then dereferencing normally - on context thread only
template<typename T> class SQLCallbackWrapper {
public:
    SQLCallbackWrapper(RefPtr<T>&& callback, ScriptExecutionContext* scriptExecutionContext)
        : m_callback(WTFMove(callback))
        , m_scriptExecutionContext(m_callback ? scriptExecutionContext : 0)
    {
        ASSERT(!m_callback || (m_scriptExecutionContext.get() && m_scriptExecutionContext->isContextThread()));
    }

    ~SQLCallbackWrapper()
    {
        clear();
    }

    void clear()
    {
        ScriptExecutionContext* scriptExecutionContextPtr;
        T* callback;
        {
            Locker locker { m_lock };
            if (!m_callback) {
                ASSERT(!m_scriptExecutionContext);
                return;
            }
            if (m_scriptExecutionContext->isContextThread()) {
                m_callback = nullptr;
                m_scriptExecutionContext = nullptr;
                return;
            }
            scriptExecutionContextPtr = m_scriptExecutionContext.leakRef();
            callback = m_callback.leakRef();
        }
        scriptExecutionContextPtr->postTask({
            ScriptExecutionContext::Task::CleanupTask,
            [callback, scriptExecutionContextPtr] (ScriptExecutionContext& context) {
                ASSERT_UNUSED(context, &context == scriptExecutionContextPtr && context.isContextThread());
                callback->deref();
                scriptExecutionContextPtr->deref();
            }
        });
    }

    RefPtr<T> unwrap()
    {
        Locker locker { m_lock };
        ASSERT(!m_callback || m_scriptExecutionContext->isContextThread());
        m_scriptExecutionContext = nullptr;
        return WTFMove(m_callback);
    }

    // Useful for optimizations only, please test the return value of unwrap to be sure.
    // FIXME: This is not thread-safe.
    bool hasCallback() const WTF_IGNORES_THREAD_SAFETY_ANALYSIS { return m_callback; }

private:
    Lock m_lock;
    RefPtr<T> m_callback WTF_GUARDED_BY_LOCK(m_lock);
    RefPtr<ScriptExecutionContext> m_scriptExecutionContext WTF_GUARDED_BY_LOCK(m_lock);
};

} // namespace WebCore
