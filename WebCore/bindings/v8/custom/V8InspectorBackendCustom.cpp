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
#include "InspectorBackend.h"

#include "DOMWindow.h"
#include "Frame.h"
#include "FrameLoader.h"
#include "ExceptionCode.h"
#include "InspectorController.h"
#include "InspectorResource.h"
#include "NotImplemented.h"
#include "Node.h"
#include "Range.h"
#include "Page.h"
#include "TextIterator.h"
#include "VisiblePosition.h"

#include "V8Binding.h"
#include "V8CustomBinding.h"
#include "V8Proxy.h"

namespace WebCore {

CALLBACK_FUNC_DECL(InspectorBackendHighlightDOMNode)
{
    INC_STATS("InspectorBackend.highlightDOMNode()");

    if (args.Length() < 1)
        return v8::Undefined();

    InspectorBackend* inspectorBackend = V8DOMWrapper::convertToNativeObject<InspectorBackend>(V8ClassIndex::INSPECTORBACKEND, args.Holder());
    inspectorBackend->highlight(args[0]->ToInt32()->Value());
    return v8::Undefined();
}

CALLBACK_FUNC_DECL(InspectorBackendSearch)
{
    INC_STATS("InspectorBackend.search()");

    if (args.Length() < 2)
        return v8::Undefined();

    Node* node = V8DOMWrapper::convertDOMWrapperToNode<Node>(v8::Handle<v8::Object>::Cast(args[0]));
    if (!node)
        return v8::Undefined();

    String target = toWebCoreStringWithNullCheck(args[1]);
    if (target.isEmpty())
        return v8::Undefined();

    v8::Local<v8::Array> result = v8::Array::New();
    RefPtr<Range> searchRange(rangeOfContents(node));

    ExceptionCode ec = 0;
    int index = 0;
    do {
        RefPtr<Range> resultRange(findPlainText(searchRange.get(), target, true, false));
        if (resultRange->collapsed(ec))
            break;

        // A non-collapsed result range can in some funky whitespace cases still not
        // advance the range's start position (4509328). Break to avoid infinite loop.
        VisiblePosition newStart = endVisiblePosition(resultRange.get(), DOWNSTREAM);
        if (newStart == startVisiblePosition(searchRange.get(), DOWNSTREAM))
            break;

        result->Set(v8::Number::New(index++), V8DOMWrapper::convertToV8Object(V8ClassIndex::RANGE, resultRange.release()));

        setStart(searchRange.get(), newStart);
    } while (true);

    return result;
}

#if ENABLE(DATABASE)
CALLBACK_FUNC_DECL(InspectorBackendDatabaseTableNames)
{
    INC_STATS("InspectorBackend.databaseTableNames()");
    v8::Local<v8::Array> result = v8::Array::New(0);
    return result;
}
#endif

CALLBACK_FUNC_DECL(InspectorBackendInspectedWindow)
{
    INC_STATS("InspectorBackend.inspectedWindow()");

    InspectorBackend* inspectorBackend = V8DOMWrapper::convertToNativeObject<InspectorBackend>(V8ClassIndex::INSPECTORBACKEND, args.Holder());
    InspectorController* ic = inspectorBackend->inspectorController();
    if (!ic)
        return v8::Undefined();
    return V8DOMWrapper::convertToV8Object<DOMWindow>(V8ClassIndex::DOMWINDOW, ic->inspectedPage()->mainFrame()->domWindow());
}

CALLBACK_FUNC_DECL(InspectorBackendSetting)
{
    INC_STATS("InspectorBackend.setting()");

    if (args.Length() < 1)
        return v8::Undefined();

    String key = toWebCoreStringWithNullCheck(args[0]);
    if (key.isEmpty())
        return v8::Undefined();

    InspectorBackend* inspectorBackend = V8DOMWrapper::convertToNativeObject<InspectorBackend>(V8ClassIndex::INSPECTORBACKEND, args.Holder());
    InspectorController* ic = inspectorBackend->inspectorController();
    if (!ic)
        return v8::Undefined();
    const InspectorController::Setting& setting = ic->setting(key);

    switch (setting.type()) {
        default:
        case InspectorController::Setting::NoType:
            return v8::Undefined();
        case InspectorController::Setting::StringType:
            return v8String(setting.string());
        case InspectorController::Setting::DoubleType:
            return v8::Number::New(setting.doubleValue());
        case InspectorController::Setting::IntegerType:
            return v8::Number::New(setting.integerValue());
        case InspectorController::Setting::BooleanType:
            return v8Boolean(setting.booleanValue());
        case InspectorController::Setting::StringVectorType: {
            const Vector<String>& strings = setting.stringVector();
            v8::Local<v8::Array> stringsArray = v8::Array::New(strings.size());
            const unsigned length = strings.size();
            for (unsigned i = 0; i < length; ++i)
                stringsArray->Set(v8::Number::New(i), v8String(strings[i]));
            return stringsArray;
        }
    }
}

CALLBACK_FUNC_DECL(InspectorBackendSetSetting)
{
    INC_STATS("InspectorBackend.setSetting()");
    if (args.Length() < 2)
        return v8::Undefined();

    String key = toWebCoreStringWithNullCheck(args[0]);
    if (key.isEmpty())
        return v8::Undefined();

    InspectorController::Setting setting;

    v8::Local<v8::Value> value = args[1];
    if (value->IsUndefined() || value->IsNull()) {
        // Do nothing. The setting is already NoType.
        ASSERT(setting.type() == InspectorController::Setting::NoType);
    } else if (value->IsString())
        setting.set(toWebCoreStringWithNullCheck(value));
    else if (value->IsNumber())
        setting.set(value->NumberValue());
    else if (value->IsBoolean())
        setting.set(value->BooleanValue());
    else if (value->IsArray()) {
        v8::Local<v8::Array> v8Array = v8::Local<v8::Array>::Cast(value);
        Vector<String> strings;
        for (unsigned i = 0; i < v8Array->Length(); ++i) {
            String item = toWebCoreString(v8Array->Get(v8::Integer::New(i)));
            if (item.isEmpty())
                return v8::Undefined();
            strings.append(item);
        }
        setting.set(strings);
    } else
        return v8::Undefined();

    InspectorBackend* inspectorBackend = V8DOMWrapper::convertToNativeObject<InspectorBackend>(V8ClassIndex::INSPECTORBACKEND, args.Holder());
    InspectorController* ic = inspectorBackend->inspectorController();
    if (ic)
        inspectorBackend->inspectorController()->setSetting(key, setting);

    return v8::Undefined();
}

CALLBACK_FUNC_DECL(InspectorBackendWrapCallback)
{
    INC_STATS("InspectorBackend.wrapCallback()");
    return args[0];
}

CALLBACK_FUNC_DECL(InspectorBackendNodeForId)
{
    INC_STATS("InspectorBackend.nodeForId()");
    if (args.Length() < 1)
        return v8::Undefined();

    InspectorBackend* inspectorBackend = V8DOMWrapper::convertToNativeObject<InspectorBackend>(V8ClassIndex::INSPECTORBACKEND, args.Holder());
    
    Node* node = inspectorBackend->nodeForId(args[0]->ToInt32()->Value());
    if (!node)
        return v8::Undefined();

    InspectorController* ic = inspectorBackend->inspectorController();
    if (!ic)
        return v8::Undefined();

    return V8DOMWrapper::convertToV8Object(V8ClassIndex::NODE, node);
}

CALLBACK_FUNC_DECL(InspectorBackendWrapObject)
{
    INC_STATS("InspectorBackend.wrapObject()");
    if (args.Length() < 2)
        return v8::Undefined();

    InspectorBackend* inspectorBackend = V8DOMWrapper::convertToNativeObject<InspectorBackend>(V8ClassIndex::INSPECTORBACKEND, args.Holder());
    return inspectorBackend->wrapObject(ScriptValue(args[0]), toWebCoreStringWithNullCheck(args[1])).v8Value();
}

CALLBACK_FUNC_DECL(InspectorBackendUnwrapObject)
{
    INC_STATS("InspectorBackend.unwrapObject()");
    if (args.Length() < 1)
        return v8::Undefined();

    InspectorBackend* inspectorBackend = V8DOMWrapper::convertToNativeObject<InspectorBackend>(V8ClassIndex::INSPECTORBACKEND, args.Holder());
    return inspectorBackend->unwrapObject(toWebCoreStringWithNullCheck(args[0])).v8Value();
}

CALLBACK_FUNC_DECL(InspectorBackendPushNodePathToFrontend)
{
    INC_STATS("InspectorBackend.pushNodePathToFrontend()");
    if (args.Length() < 2)
        return v8::Undefined();

    InspectorBackend* inspectorBackend = V8DOMWrapper::convertToNativeObject<InspectorBackend>(V8ClassIndex::INSPECTORBACKEND, args.Holder());
    Node* node = V8DOMWrapper::convertDOMWrapperToNode<Node>(v8::Handle<v8::Object>::Cast(args[0]));
    bool selectInUI = args[1]->ToBoolean()->Value();
    if (node)
        return v8::Number::New(inspectorBackend->pushNodePathToFrontend(node, selectInUI));

    return v8::Undefined();
}

#if ENABLE(DATABASE)
CALLBACK_FUNC_DECL(InspectorBackendSelectDatabase)
{
    INC_STATS("InspectorBackend.selectDatabase()");
    if (args.Length() < 1)
        return v8::Undefined();

    InspectorBackend* inspectorBackend = V8DOMWrapper::convertToNativeObject<InspectorBackend>(V8ClassIndex::INSPECTORBACKEND, args.Holder());
    Database* database = V8DOMWrapper::convertToNativeObject<Database>(V8ClassIndex::DATABASE, v8::Handle<v8::Object>::Cast(args[0]));
    if (database)
        inspectorBackend->selectDatabase(database);

    return v8::Undefined();
}
#endif

#if ENABLE(DOM_STORAGE)
CALLBACK_FUNC_DECL(InspectorBackendSelectDOMStorage)
{
    INC_STATS("InspectorBackend.selectDOMStorage()");
    if (args.Length() < 1)
        return v8::Undefined();

    InspectorBackend* inspectorBackend = V8DOMWrapper::convertToNativeObject<InspectorBackend>(V8ClassIndex::INSPECTORBACKEND, args.Holder());
    Storage* storage = V8DOMWrapper::convertToNativeObject<Storage>(V8ClassIndex::STORAGE, v8::Handle<v8::Object>::Cast(args[0]));
    if (storage)
        inspectorBackend->selectDOMStorage(storage);

    return v8::Undefined();
}
#endif

} // namespace WebCore
