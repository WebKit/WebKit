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

#ifndef V8MutationCallback_h
#define V8MutationCallback_h

#include "ActiveDOMCallback.h"
#include "DOMWrapperWorld.h"
#include "MutationCallback.h"
#include "ScopedPersistent.h"
#include "V8Utilities.h"
#include <v8.h>
#include <wtf/RefPtr.h>

namespace WebCore {

class ScriptExecutionContext;

class V8MutationCallback : public MutationCallback, public ActiveDOMCallback {
    friend class WeakHandleListener<V8MutationCallback>;
public:
    static PassRefPtr<V8MutationCallback> create(v8::Handle<v8::Function> callback, ScriptExecutionContext* context, v8::Handle<v8::Object> owner, v8::Isolate* isolate)
    {
        ASSERT(context);
        return adoptRef(new V8MutationCallback(callback, context, owner, isolate));
    }

    virtual void call(const Vector<RefPtr<MutationRecord> >&, MutationObserver*) OVERRIDE;
    virtual ScriptExecutionContext* scriptExecutionContext() const OVERRIDE { return ContextDestructionObserver::scriptExecutionContext(); }

private:
    V8MutationCallback(v8::Handle<v8::Function>, ScriptExecutionContext*, v8::Handle<v8::Object>, v8::Isolate*);

    ScopedPersistent<v8::Function> m_callback;
    RefPtr<DOMWrapperWorld> m_world;
};

}

#endif // V8MutationCallback_h
