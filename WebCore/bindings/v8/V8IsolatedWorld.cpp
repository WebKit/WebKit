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

#include "V8DOMWindow.h"
#include "Frame.h"
#include "HashMap.h"
#include "ScriptController.h"

namespace WebCore {

// We cache a pointer to the V8IsolatedWorld as a hidden property on the
// context's global object.  This constant is the name of our hidden property.
//
// FIXME: We should centralize our list of V8 hidden proprety names to ensure
// we don't have name collisions.
//
static char isolatedWorldKey[] = "IsolatedWorld";

static v8::Handle<v8::String> getIsolatedWorldKey()
{
    static v8::Persistent<v8::String>* key;
    if (!key)
        key = new v8::Persistent<v8::String>(v8::Persistent<v8::String>::New(v8::String::NewSymbol(isolatedWorldKey)));
    return *key;
}

static int isolatedWorldCount = 0;

static void contextWeakReferenceCallback(v8::Persistent<v8::Value> object, void* isolated_world)
{
    // Our context is going away.  Time to clean up the world.
    V8IsolatedWorld* world = static_cast<V8IsolatedWorld*>(isolated_world);
    delete world;
}

void V8IsolatedWorld::evaluate(const Vector<ScriptSourceCode>& sources, V8Proxy* proxy)
{
    v8::HandleScope scope;
    v8::Persistent<v8::Context> context = proxy->createNewContext(v8::Handle<v8::Object>());

    // Run code in the new context.
    v8::Context::Scope context_scope(context);

    // The lifetime of this object is controlled by the V8 GC.
    // We need to create the world before touching DOM wrappers.
    V8IsolatedWorld* world = new V8IsolatedWorld(context);

    proxy->installDOMWindow(context, proxy->frame()->domWindow());
    for (size_t i = 0; i < sources.size(); ++i)
        proxy->evaluate(sources[i], 0);

    // Using the default security token means that the canAccess is always
    // called, which is slow.
    // FIXME: Use tokens where possible. This will mean keeping track of all
    //        created contexts so that they can all be updated when the
    //        document domain
    //        changes.
    // FIXME: Move this statement above proxy->evaluate?  It seems like we
    //        should set up the token before running the script.
    context->UseDefaultSecurityToken();

    context.Dispose();
    // WARNING!  This might well delete |world|.
}

V8IsolatedWorld::V8IsolatedWorld(v8::Handle<v8::Context> context)
    : m_context(context)
{
    ++isolatedWorldCount;
    m_context.MakeWeak(this, &contextWeakReferenceCallback);
    m_context->Global()->SetHiddenValue(getIsolatedWorldKey(), v8::External::Wrap(this));
}

V8IsolatedWorld::~V8IsolatedWorld()
{
    --isolatedWorldCount;
}

V8IsolatedWorld* V8IsolatedWorld::getEntered()
{
    if (isolatedWorldCount == 0) {
        // This is a temporary performance optimization.   Essentially,
        // GetHiddenValue is too slow for this code path.  We need to get the
        // V8 team to add a real property to v8::Context for isolated worlds.
        // Until then, we optimize the common case of not having any isolated
        // worlds at all.
        return 0;
    }

    if (!v8::Context::InContext())
        return 0;
    v8::HandleScope scope;

    v8::Local<v8::Value> world = v8::Context::GetEntered()->Global()->GetHiddenValue(getIsolatedWorldKey());
    if (world.IsEmpty())
        return 0;

    return static_cast<V8IsolatedWorld*>(v8::External::Unwrap(world));
}

} // namespace WebCore
