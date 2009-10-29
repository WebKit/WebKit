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

#include "V8IsolatedWorld.h"

#include <v8.h>

#include "Frame.h"
#include "FrameLoaderClient.h"
#include "HashMap.h"
#include "ScriptController.h"
#include "V8DOMWindow.h"
#include "V8HiddenPropertyName.h"

namespace WebCore {

int V8IsolatedWorld::isolatedWorldCount = 0;

void V8IsolatedWorld::contextWeakReferenceCallback(v8::Persistent<v8::Value> object, void* isolated_world)
{
    // Our context is going away.  Time to clean up the world.
    V8IsolatedWorld* world = static_cast<V8IsolatedWorld*>(isolated_world);
    delete world;
}

V8IsolatedWorld::V8IsolatedWorld(V8Proxy* proxy, int extensionGroup)
{
    ++isolatedWorldCount;

    v8::HandleScope scope;
    m_context = SharedPersistent<v8::Context>::create(proxy->createNewContext(v8::Handle<v8::Object>(), extensionGroup));

    // Run code in the new context.
    v8::Context::Scope context_scope(m_context->get());

    m_context->get()->Global()->SetHiddenValue(V8HiddenPropertyName::isolatedWorld(), v8::External::Wrap(this));

    V8Proxy::installHiddenObjectPrototype(m_context->get());
    proxy->installDOMWindow(m_context->get(), proxy->frame()->domWindow());

    // Using the default security token means that the canAccess is always
    // called, which is slow.
    // FIXME: Use tokens where possible. This will mean keeping track of all
    //        created contexts so that they can all be updated when the
    //        document domain
    //        changes.
    m_context->get()->UseDefaultSecurityToken();

    proxy->frame()->loader()->client()->didCreateIsolatedScriptContext();
}

void V8IsolatedWorld::destroy()
{
    m_context->get().MakeWeak(this, &contextWeakReferenceCallback);
}

V8IsolatedWorld::~V8IsolatedWorld()
{
    --isolatedWorldCount;
    m_context->disposeHandle();
}

V8IsolatedWorld* V8IsolatedWorld::getEnteredImpl()
{
    if (!v8::Context::InContext())
        return 0;
    v8::HandleScope scope;

    v8::Local<v8::Value> world = v8::Context::GetEntered()->Global()->GetHiddenValue(V8HiddenPropertyName::isolatedWorld());
    if (world.IsEmpty())
        return 0;

    return static_cast<V8IsolatedWorld*>(v8::External::Unwrap(world));
}

} // namespace WebCore
