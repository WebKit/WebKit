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
#include "ScriptObjectQuarantine.h"

#include "Database.h"
#include "Document.h"
#include "DOMWindow.h"
#include "Frame.h"
#include "Page.h"
#include "ScriptObject.h"
#include "ScriptValue.h"

#include "V8Binding.h"
#include "V8Proxy.h"

#include <v8.h>

namespace WebCore {

ScriptValue quarantineValue(ScriptState* scriptState, const ScriptValue& value)
{
    return value;
}

bool getQuarantinedScriptObject(Database* database, ScriptObject& quarantinedObject)
{
    ASSERT(database);

    // FIXME: Implement when Database V8 bindings are enabled
    ASSERT_NOT_REACHED();
    quarantinedObject = ScriptObject();
    return false;
}

bool getQuarantinedScriptObject(Frame* frame, Storage* storage, ScriptObject& quarantinedObject)
{
    ASSERT(frame);
    ASSERT(storage);

    // FIXME: Implement when DOM Storage V8 bindings are enabled
    ASSERT_NOT_REACHED();
    quarantinedObject = ScriptObject();
    return true;
}

bool getQuarantinedScriptObject(Node* node, ScriptObject& quarantinedObject)
{
    ASSERT(node);

    v8::HandleScope handleScope;
    v8::Local<v8::Context> context = V8Proxy::GetContext(node->document()->page()->mainFrame());
    v8::Context::Scope scope(context);

    v8::Handle<v8::Value> v8Node = V8Proxy::NodeToV8Object(node);
    quarantinedObject = ScriptObject(v8::Local<v8::Object>(v8::Object::Cast(*v8Node)));

    return true;
}

bool getQuarantinedScriptObject(DOMWindow* domWindow, ScriptObject& quarantinedObject)
{
    ASSERT(domWindow);

    v8::HandleScope handleScope;
    v8::Local<v8::Context> context = V8Proxy::GetContext(domWindow->frame());
    v8::Context::Scope scope(context);

    v8::Handle<v8::Value> v8DomWindow = V8Proxy::ToV8Object(V8ClassIndex::DOMWINDOW, domWindow);
    quarantinedObject = ScriptObject(v8::Local<v8::Object>(v8::Object::Cast(*v8DomWindow)));

    return true;
}


} // namespace WebCore
