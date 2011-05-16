/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef CallbackTask_h
#define CallbackTask_h

#if ENABLE(MEDIA_STREAM)

#include "ScriptExecutionContext.h"
#include <wtf/Forward.h>
#include <wtf/PassRefPtr.h>
#include <wtf/RefPtr.h>

namespace WebCore {

// Helper template to schedule calls to callbacks using their own script execution context.
// CallbackType is assumed to implement Scheduler and to be reference-counted.
template <typename CallbackType, typename ArgumentType>
class CallbackTask1 : public ScriptExecutionContext::Task {
public:
    static PassOwnPtr<CallbackTask1> create(PassRefPtr<CallbackType> callback, PassRefPtr<ArgumentType> data)
    {
        return adoptPtr(new CallbackTask1(callback, data));
    }

    virtual void performTask(ScriptExecutionContext*)
    {
        m_callback->handleEvent(m_data.get());
    }

    class Scheduler {
    public:
        bool scheduleCallback(ScriptExecutionContext* context, PassRefPtr<ArgumentType> data)
        {
            if (context) {
                context->postTask(CallbackTask1<CallbackType, ArgumentType>::create(static_cast<CallbackType*>(this), data));
                return true;
            }
            return false;
        }
    };

private:
    CallbackTask1(PassRefPtr<CallbackType> callback, PassRefPtr<ArgumentType> data)
        : m_callback(callback)
        , m_data(data)
    {
    }

    RefPtr<CallbackType> m_callback;
    RefPtr<ArgumentType> m_data;
};

}

#endif // ENABLE(MEDIA_STREAM)

#endif // CallbackTask_h
