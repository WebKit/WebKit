/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
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

#ifndef V8CustomIDBCallback_h
#define V8CustomIDBCallback_h

#include "Frame.h"
#include "V8CustomVoidCallback.h"
#include <v8.h>
#include <wtf/PassRefPtr.h>
#include <wtf/RefCounted.h>
#include <wtf/RefPtr.h>

#if ENABLE(INDEXED_DATABASE)

namespace WebCore {

// FIXME: Maybe split common parts into a base class.
template <typename CallbackType>
class V8CustomIDBCallback : public RefCounted<V8CustomIDBCallback<CallbackType> > {
public:
    static PassRefPtr<V8CustomIDBCallback> create(v8::Local<v8::Value> value, Frame* frame)
    {
        ASSERT(value->IsObject());
        return adoptRef(new V8CustomIDBCallback(value->ToObject(), frame));
    }

    virtual ~V8CustomIDBCallback()
    {
        m_callback.Dispose();
    }

    virtual void handleEvent(CallbackType* result)
    {
        v8::HandleScope handleScope;
        v8::Handle<v8::Context> context = V8Proxy::context(m_frame.get());
        if (context.IsEmpty())
            return;

        v8::Context::Scope scope(context);
        v8::Handle<v8::Value> argv[] = {
            toV8(result)
        };
        RefPtr<Frame> protector(m_frame);
        bool callbackReturnValue = false;
        // FIXME: Do we care if this thing returns true (i.e. it raised an exception)?
        invokeCallback(m_callback, 1, argv, callbackReturnValue);
    }

private:
    V8CustomIDBCallback(v8::Local<v8::Object>callback, Frame* frame)
        : m_callback(v8::Persistent<v8::Object>::New(callback)),
          m_frame(frame)
    {
    }

    v8::Persistent<v8::Object> m_callback;
    RefPtr<Frame> m_frame;
};

}

#endif

#endif // V8CustomIDBCallback_h
