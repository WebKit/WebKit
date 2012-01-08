/*
 * Copyright (C) 2009 Google Inc. All rights reserved.
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
#include "V8BindingState.h"

#include "Frame.h"
#include "ScriptController.h"
#include "V8Proxy.h"
#include <wtf/StdLibExtras.h>

namespace WebCore {

State<V8Binding>* State<V8Binding>::Only()
{
    DEFINE_STATIC_LOCAL(State, globalV8BindingState, ());
    return &globalV8BindingState;
}

DOMWindow* State<V8Binding>::activeWindow()
{
    v8::Local<v8::Context> activeContext = v8::Context::GetCalling();
    if (activeContext.IsEmpty()) {
        // There is a single activation record on the stack, so that must
        // be the activeContext.
        activeContext = v8::Context::GetCurrent();
    }
    return V8Proxy::retrieveWindow(activeContext);
}

DOMWindow* State<V8Binding>::firstWindow()
{
    return V8Proxy::retrieveWindow(v8::Context::GetEntered());
}

Frame* State<V8Binding>::activeFrame()
{
    Frame* frame = V8Proxy::retrieveFrameForCallingContext();
    if (!frame) {
        // Unfortunately, when processing script from a plug-in, we might not
        // have a calling context.  In those cases, we fall back to the
        // entered context for security checks.
        // FIXME: We need a better API for retrieving frames that abstracts
        //        away this concern.
        frame = V8Proxy::retrieveFrameForEnteredContext();
    }
    return frame;
}

Frame* State<V8Binding>::firstFrame()
{
    return V8Proxy::retrieveFrameForEnteredContext();
}

void State<V8Binding>::immediatelyReportUnsafeAccessTo(Frame* target)
{
    V8Proxy::reportUnsafeAccessTo(target);
}

} // namespace WebCore
