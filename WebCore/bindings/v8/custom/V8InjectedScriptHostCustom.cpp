/*
 * Copyright (C) 2007-2009 Google Inc. All rights reserved.
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
#include "InjectedScriptHost.h"

#include "Database.h"
#include "DOMWindow.h"
#include "Frame.h"
#include "InspectorController.h"
#include "Node.h"
#include "Page.h"

#include "V8Binding.h"
#include "V8CustomBinding.h"
#include "V8Proxy.h"

namespace WebCore {

CALLBACK_FUNC_DECL(InjectedScriptHostInspectedWindow)
{
    INC_STATS("InjectedScriptHost.inspectedWindow()");

    InjectedScriptHost* host = V8DOMWrapper::convertToNativeObject<InjectedScriptHost>(V8ClassIndex::INJECTEDSCRIPTHOST, args.Holder());
    InspectorController* ic = host->inspectorController();
    if (!ic)
        return v8::Undefined();
    return V8DOMWrapper::convertToV8Object<DOMWindow>(V8ClassIndex::DOMWINDOW, ic->inspectedPage()->mainFrame()->domWindow());
}

CALLBACK_FUNC_DECL(InjectedScriptHostWrapCallback)
{
    INC_STATS("InjectedScriptHost.wrapCallback()");
    return args[0];
}

CALLBACK_FUNC_DECL(InjectedScriptHostNodeForId)
{
    INC_STATS("InjectedScriptHost.nodeForId()");
    if (args.Length() < 1)
        return v8::Undefined();

    InjectedScriptHost* host = V8DOMWrapper::convertToNativeObject<InjectedScriptHost>(V8ClassIndex::INJECTEDSCRIPTHOST, args.Holder());
    
    Node* node = host->nodeForId(args[0]->ToInt32()->Value());
    if (!node)
        return v8::Undefined();

    InspectorController* ic = host->inspectorController();
    if (!ic)
        return v8::Undefined();

    return V8DOMWrapper::convertToV8Object(V8ClassIndex::NODE, node);
}

CALLBACK_FUNC_DECL(InjectedScriptHostWrapObject)
{
    INC_STATS("InjectedScriptHost.wrapObject()");
    if (args.Length() < 2)
        return v8::Undefined();

    InjectedScriptHost* host = V8DOMWrapper::convertToNativeObject<InjectedScriptHost>(V8ClassIndex::INJECTEDSCRIPTHOST, args.Holder());
    return host->wrapObject(ScriptValue(args[0]), toWebCoreStringWithNullCheck(args[1])).v8Value();
}

CALLBACK_FUNC_DECL(InjectedScriptHostUnwrapObject)
{
    INC_STATS("InjectedScriptHost.unwrapObject()");
    if (args.Length() < 1)
        return v8::Undefined();

    InjectedScriptHost* host = V8DOMWrapper::convertToNativeObject<InjectedScriptHost>(V8ClassIndex::INJECTEDSCRIPTHOST, args.Holder());
    return host->unwrapObject(toWebCoreStringWithNullCheck(args[0])).v8Value();
}

CALLBACK_FUNC_DECL(InjectedScriptHostPushNodePathToFrontend)
{
    INC_STATS("InjectedScriptHost.pushNodePathToFrontend()");
    if (args.Length() < 2)
        return v8::Undefined();

    InjectedScriptHost* host = V8DOMWrapper::convertToNativeObject<InjectedScriptHost>(V8ClassIndex::INJECTEDSCRIPTHOST, args.Holder());
    Node* node = V8DOMWrapper::convertDOMWrapperToNode<Node>(v8::Handle<v8::Object>::Cast(args[0]));
    bool selectInUI = args[1]->ToBoolean()->Value();
    if (node)
        return v8::Number::New(host->pushNodePathToFrontend(node, selectInUI));

    return v8::Undefined();
}

#if ENABLE(DATABASE)
CALLBACK_FUNC_DECL(InjectedScriptHostDatabaseForId)
{
    INC_STATS("InjectedScriptHost.databaseForId()");
    if (args.Length() < 1)
        return v8::Undefined();

    InjectedScriptHost* host = V8DOMWrapper::convertToNativeObject<InjectedScriptHost>(V8ClassIndex::INJECTEDSCRIPTHOST, args.Holder());
    Database* database = host->databaseForId(args[0]->ToInt32()->Value());
    if (!database)
        return v8::Undefined();
    return V8DOMWrapper::convertToV8Object<Database>(V8ClassIndex::DATABASE, database);
}

CALLBACK_FUNC_DECL(InjectedScriptHostSelectDatabase)
{
    INC_STATS("InjectedScriptHost.selectDatabase()");
    if (args.Length() < 1)
        return v8::Undefined();

    InjectedScriptHost* host = V8DOMWrapper::convertToNativeObject<InjectedScriptHost>(V8ClassIndex::INJECTEDSCRIPTHOST, args.Holder());
    Database* database = V8DOMWrapper::convertToNativeObject<Database>(V8ClassIndex::DATABASE, v8::Handle<v8::Object>::Cast(args[0]));
    if (database)
        host->selectDatabase(database);

    return v8::Undefined();
}
#endif

#if ENABLE(DOM_STORAGE)
CALLBACK_FUNC_DECL(InjectedScriptHostSelectDOMStorage)
{
    INC_STATS("InjectedScriptHost.selectDOMStorage()");
    if (args.Length() < 1)
        return v8::Undefined();

    InjectedScriptHost* host = V8DOMWrapper::convertToNativeObject<InjectedScriptHost>(V8ClassIndex::INJECTEDSCRIPTHOST, args.Holder());
    Storage* storage = V8DOMWrapper::convertToNativeObject<Storage>(V8ClassIndex::STORAGE, v8::Handle<v8::Object>::Cast(args[0]));
    if (storage)
        host->selectDOMStorage(storage);

    return v8::Undefined();
}
#endif

} // namespace WebCore
