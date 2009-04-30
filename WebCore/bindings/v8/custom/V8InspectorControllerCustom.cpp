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
#include "InspectorController.h"

#include "DOMWindow.h"
#include "Frame.h"
#include "FrameLoader.h"
#include "ExceptionCode.h"
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

CALLBACK_FUNC_DECL(InspectorControllerHighlightDOMNode)
{
    INC_STATS("InspectorController.highlightDOMNode()");

    if (args.Length() < 1)
        return v8::Undefined();

    Node* node = V8Proxy::DOMWrapperToNode<Node>(args[0]);
    if (!node)
        return v8::Undefined();

    InspectorController* inspectorController = V8Proxy::ToNativeObject<InspectorController>(V8ClassIndex::INSPECTORCONTROLLER, args.Holder());
    inspectorController->highlight(node);

    return v8::Undefined();
}

CALLBACK_FUNC_DECL(InspectorControllerAddResourceSourceToFrame)
{
    INC_STATS("InspectorController.addResourceSourceToFrame()");

    if (args.Length() < 2)
        return v8::Undefined();

    if (!args[0]->IsNumber())
        return v8::Undefined();

    long long identifier = static_cast<long long>(args[0]->NumberValue());

    InspectorController* inspectorController = V8Proxy::ToNativeObject<InspectorController>(V8ClassIndex::INSPECTORCONTROLLER, args.Holder());
    RefPtr<InspectorResource> resource = inspectorController->resources().get(identifier);
    ASSERT(resource);
    if (!resource)
        return v8::Undefined();

    String sourceString = resource->sourceString();
    if (sourceString.isEmpty())
        return v8::Undefined();

    Node* node = V8Proxy::DOMWrapperToNode<Node>(args[1]);
    if (!node)
        return v8::Undefined();

    return v8Boolean(inspectorController->addSourceToFrame(resource->mimeType(), sourceString, node));
}

CALLBACK_FUNC_DECL(InspectorControllerAddSourceToFrame)
{
    INC_STATS("InspectorController.addSourceToFrame()");

    if (args.Length() < 2)
        return v8::Undefined();

    v8::TryCatch exceptionCatcher;

    String mimeType = toWebCoreStringWithNullCheck(args[0]);
    if (mimeType.isEmpty() || exceptionCatcher.HasCaught())
        return v8::Undefined();

    String sourceString = toWebCoreStringWithNullCheck(args[1]);
    if (sourceString.isEmpty() || exceptionCatcher.HasCaught())
        return v8::Undefined();

    Node* node = V8Proxy::DOMWrapperToNode<Node>(args[1]);
    if (!node)
        return v8::Undefined();

    InspectorController* inspectorController = V8Proxy::ToNativeObject<InspectorController>(V8ClassIndex::INSPECTORCONTROLLER, args.Holder());
    return v8Boolean(inspectorController->addSourceToFrame(mimeType, sourceString, node));
}

CALLBACK_FUNC_DECL(InspectorControllerGetResourceDocumentNode)
{
    INC_STATS("InspectorController.getResourceDocumentNode()");

    if (args.Length() < 1)
        return v8::Undefined();

    if (!args[1]->IsNumber())
        return v8::Undefined();

    unsigned identifier = args[1]->Int32Value();

    InspectorController* inspectorController = V8Proxy::ToNativeObject<InspectorController>(V8ClassIndex::INSPECTORCONTROLLER, args.Holder());
    RefPtr<InspectorResource> resource = inspectorController->resources().get(identifier);
    ASSERT(resource);
    if (!resource)
        return v8::Undefined();

    Frame* frame = resource->frame();
    Document* document = frame->document();

    if (document->isPluginDocument() || document->isImageDocument() || document->isMediaDocument())
        return v8::Undefined();

    return V8Proxy::ToV8Object(V8ClassIndex::DOCUMENT, document);
}

CALLBACK_FUNC_DECL(InspectorControllerSearch)
{
    INC_STATS("InspectorController.search()");

    if (args.Length() < 2)
        return v8::Undefined();

    Node* node = V8Proxy::DOMWrapperToNode<Node>(args[0]);
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

        result->Set(v8::Number::New(index++), V8Proxy::ToV8Object<Range>(V8ClassIndex::RANGE, resultRange.get()));

        setStart(searchRange.get(), newStart);
    } while (true);

    return result;
}

#if ENABLE(DATABASE)
CALLBACK_FUNC_DECL(InspectorControllerDatabaseTableNames)
{
    INC_STATS("InspectorController.databaseTableNames()");
    v8::Local<v8::Array> result = v8::Array::New(0);
    return result;
}
#endif

CALLBACK_FUNC_DECL(InspectorControllerInspectedWindow)
{
    INC_STATS("InspectorController.inspectedWindow()");

    InspectorController* inspectorController = V8Proxy::ToNativeObject<InspectorController>(V8ClassIndex::INSPECTORCONTROLLER, args.Holder());
    return V8Proxy::ToV8Object<DOMWindow>(V8ClassIndex::DOMWINDOW, inspectorController->inspectedPage()->mainFrame()->domWindow());
}

CALLBACK_FUNC_DECL(InspectorControllerSetting)
{
    INC_STATS("InspectorController.setting()");

    if (args.Length() < 1)
        return v8::Undefined();

    String key = toWebCoreStringWithNullCheck(args[0]);
    if (key.isEmpty())
        return v8::Undefined();

    InspectorController* inspectorController = V8Proxy::ToNativeObject<InspectorController>(V8ClassIndex::INSPECTORCONTROLLER, args.Holder());
    const InspectorController::Setting& setting = inspectorController ->setting(key);

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

CALLBACK_FUNC_DECL(InspectorControllerSetSetting)
{
    INC_STATS("InspectorController.setSetting()");
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

    InspectorController* inspectorController = V8Proxy::ToNativeObject<InspectorController>(V8ClassIndex::INSPECTORCONTROLLER, args.Holder());
    inspectorController->setSetting(key, setting);

    return v8::Undefined();
}

CALLBACK_FUNC_DECL(InspectorControllerWrapCallback)
{
    INC_STATS("InspectorController.wrapCallback()");
    return args[0];
}

} // namespace WebCore
