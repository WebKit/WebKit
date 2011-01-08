/*
 * Copyright 2009, The Android Open Source Project
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *  * Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *  * Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef V8CustomPositionErrorCallback_h
#define V8CustomPositionErrorCallback_h

#include "PositionErrorCallback.h"

#include <v8.h>
#include <wtf/PassRefPtr.h>
#include <wtf/RefPtr.h>

namespace WebCore {

class Frame;
class PositionError;
class ScriptExecutionContext;

class V8CustomPositionErrorCallback : public PositionErrorCallback {
public:
    static PassRefPtr<V8CustomPositionErrorCallback> create(v8::Local<v8::Value> value, ScriptExecutionContext* context)
    {
        ASSERT(value->IsObject());
        return adoptRef(new V8CustomPositionErrorCallback(value->ToObject(), context));
    }
    virtual ~V8CustomPositionErrorCallback();

    virtual void handleEvent(PositionError*);

private:
    V8CustomPositionErrorCallback(v8::Local<v8::Object>, ScriptExecutionContext*);

    v8::Persistent<v8::Object> m_callback;
};

} // namespace WebCore

#endif // V8CustomPositionErrorCallback_h
