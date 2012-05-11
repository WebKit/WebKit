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

#include "V8IsolatedContext.h"

#include "Frame.h"
#include "FrameLoaderClient.h"
#include "InspectorInstrumentation.h"
#include "SecurityOrigin.h"
#include "V8BindingPerContextData.h"
#include "V8DOMWindow.h"
#include "V8Proxy.h"
#include <wtf/StringExtras.h>

namespace WebCore {

V8IsolatedContext* V8IsolatedContext::isolatedContext()
{
    return reinterpret_cast<V8IsolatedContext*>(getGlobalObject(v8::Context::GetEntered())->GetPointerFromInternalField(V8DOMWindow::enteredIsolatedWorldIndex));
}

void V8IsolatedContext::contextWeakReferenceCallback(v8::Persistent<v8::Value> object, void* isolatedContext)
{
    // Our context is going away.  Time to clean up the world.
    V8IsolatedContext* context = static_cast<V8IsolatedContext*>(isolatedContext);
    delete context;
}

static void setInjectedScriptContextDebugId(v8::Handle<v8::Context> targetContext, int debugId)
{
    char buffer[32];
    if (debugId == -1)
        snprintf(buffer, sizeof(buffer), "injected");
    else
        snprintf(buffer, sizeof(buffer), "injected,%d", debugId);
    targetContext->SetData(v8::String::New(buffer));
}

V8IsolatedContext::V8IsolatedContext(V8Proxy* proxy, int extensionGroup, int worldId)
    : m_world(IsolatedWorld::create(worldId)),
      m_frame(proxy->frame())
{
    v8::HandleScope scope;
    v8::Handle<v8::Context> mainWorldContext = proxy->windowShell()->context();
    if (mainWorldContext.IsEmpty())
        return;

    // FIXME: We should be creating a new V8DOMWindowShell here instead of riping out the context.
    m_context = SharedPersistent<v8::Context>::create(proxy->windowShell()->createNewContext(v8::Handle<v8::Object>(), extensionGroup, m_world->id()));
    if (m_context->get().IsEmpty())
        return;

    // Run code in the new context.
    v8::Context::Scope contextScope(m_context->get());

    // Setup context id for JS debugger.
    setInjectedScriptContextDebugId(m_context->get(), proxy->contextDebugId(mainWorldContext));

    getGlobalObject(m_context->get())->SetPointerInInternalField(V8DOMWindow::enteredIsolatedWorldIndex, this);

    m_perContextData = V8BindingPerContextData::create(m_context->get());
    m_perContextData->init();

    // FIXME: This will go away once we have a windowShell for the isolated world.
    proxy->windowShell()->installDOMWindow(m_context->get(), m_frame->domWindow());

    // Using the default security token means that the canAccess is always
    // called, which is slow.
    // FIXME: Use tokens where possible. This will mean keeping track of all
    //        created contexts so that they can all be updated when the
    //        document domain
    //        changes.
    m_context->get()->UseDefaultSecurityToken();

    m_frame->loader()->client()->didCreateScriptContext(context(), extensionGroup, m_world->id());
}

void V8IsolatedContext::destroy()
{
    m_perContextData.clear();
    m_frame->loader()->client()->willReleaseScriptContext(context(), m_world->id());
    m_context->get().MakeWeak(this, &contextWeakReferenceCallback);
    m_frame = 0;
}

V8IsolatedContext::~V8IsolatedContext()
{
    m_context->disposeHandle();
}

void V8IsolatedContext::setSecurityOrigin(PassRefPtr<SecurityOrigin> securityOrigin)
{
    if (!m_securityOrigin && InspectorInstrumentation::hasFrontends() && !context().IsEmpty()) {
        v8::HandleScope handleScope;
        ScriptState* scriptState = ScriptState::forContext(v8::Local<v8::Context>::New(context()));
        InspectorInstrumentation::didCreateIsolatedContext(m_frame, scriptState, securityOrigin.get());
    }
    m_securityOrigin = securityOrigin;
}

} // namespace WebCore
