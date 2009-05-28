/*
 * Copyright (C) 2007, 2008 Apple Inc. All rights reserved.
 * Copyright (C) 2008 Matt Lilek <webkit@mattlilek.com>
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
#include "JSInspectorController.h"

#include "Console.h"
#if ENABLE(DATABASE)
#include "Database.h"
#include "JSDatabase.h"
#endif
#include "ExceptionCode.h"
#include "Frame.h"
#include "FrameLoader.h"
#include "InspectorController.h"
#include "InspectorResource.h"
#include "JSDOMWindow.h"
#include "JSInspectedObjectWrapper.h"
#include "JSInspectorCallbackWrapper.h"
#include "JSNode.h"
#include "JSRange.h"
#include "Node.h"
#include "Page.h"
#include "TextIterator.h"
#include "VisiblePosition.h"
#include <runtime/JSArray.h>
#include <runtime/JSLock.h>
#include <wtf/Vector.h>

#if ENABLE(JAVASCRIPT_DEBUGGER)
#include "JavaScriptCallFrame.h"
#include "JavaScriptDebugServer.h"
#include "JavaScriptProfile.h"
#include "JSJavaScriptCallFrame.h"
#include <profiler/Profile.h>
#include <profiler/Profiler.h>
#endif

using namespace JSC;

namespace WebCore {

JSValue JSInspectorController::highlightDOMNode(JSC::ExecState*, const JSC::ArgList& args)
{
    if (args.size() < 1)
        return jsUndefined();

    JSQuarantinedObjectWrapper* wrapper = JSQuarantinedObjectWrapper::asWrapper(args.at(0));
    if (!wrapper)
        return jsUndefined();

    Node* node = toNode(wrapper->unwrappedObject());
    if (!node)
        return jsUndefined();

    impl()->highlight(node);

    return jsUndefined();
}

JSValue JSInspectorController::getResourceDocumentNode(ExecState* exec, const ArgList& args)
{
    if (args.size() < 1)
        return jsUndefined();

    bool ok = false;
    unsigned identifier = args.at(0).toUInt32(exec, ok);
    if (!ok)
        return jsUndefined();

    RefPtr<InspectorResource> resource = impl()->resources().get(identifier);
    ASSERT(resource);
    if (!resource)
        return jsUndefined();

    Frame* frame = resource->frame();
    Document* document = frame->document();

    if (document->isPluginDocument() || document->isImageDocument() || document->isMediaDocument())
        return jsUndefined();

    ExecState* resourceExec = toJSDOMWindowShell(frame)->window()->globalExec();

    JSLock lock(false);
    return JSInspectedObjectWrapper::wrap(resourceExec, toJS(resourceExec, document));
}

JSValue JSInspectorController::search(ExecState* exec, const ArgList& args)
{
    if (args.size() < 2)
        return jsUndefined();

    Node* node = toNode(args.at(0));
    if (!node)
        return jsUndefined();

    String target = args.at(1).toString(exec);
    if (exec->hadException())
        return jsUndefined();

    MarkedArgumentBuffer result;
    RefPtr<Range> searchRange(rangeOfContents(node));

    ExceptionCode ec = 0;
    do {
        RefPtr<Range> resultRange(findPlainText(searchRange.get(), target, true, false));
        if (resultRange->collapsed(ec))
            break;

        // A non-collapsed result range can in some funky whitespace cases still not
        // advance the range's start position (4509328). Break to avoid infinite loop.
        VisiblePosition newStart = endVisiblePosition(resultRange.get(), DOWNSTREAM);
        if (newStart == startVisiblePosition(searchRange.get(), DOWNSTREAM))
            break;

        result.append(toJS(exec, resultRange.get()));

        setStart(searchRange.get(), newStart);
    } while (true);

    return constructArray(exec, result);
}

#if ENABLE(DATABASE)
JSValue JSInspectorController::databaseTableNames(ExecState* exec, const ArgList& args)
{
    if (args.size() < 1)
        return jsUndefined();

    JSQuarantinedObjectWrapper* wrapper = JSQuarantinedObjectWrapper::asWrapper(args.at(0));
    if (!wrapper)
        return jsUndefined();

    Database* database = toDatabase(wrapper->unwrappedObject());
    if (!database)
        return jsUndefined();

    MarkedArgumentBuffer result;

    Vector<String> tableNames = database->tableNames();
    unsigned length = tableNames.size();
    for (unsigned i = 0; i < length; ++i)
        result.append(jsString(exec, tableNames[i]));

    return constructArray(exec, result);
}
#endif

JSValue JSInspectorController::inspectedWindow(ExecState*, const ArgList&)
{
    JSDOMWindow* inspectedWindow = toJSDOMWindow(impl()->inspectedPage()->mainFrame());
    return JSInspectedObjectWrapper::wrap(inspectedWindow->globalExec(), inspectedWindow);
}

JSValue JSInspectorController::setting(ExecState* exec, const ArgList& args)
{
    if (args.size() < 1)
        return jsUndefined();

    String key = args.at(0).toString(exec);
    if (exec->hadException())
        return jsUndefined();

    const InspectorController::Setting& setting = impl()->setting(key);

    switch (setting.type()) {
        default:
        case InspectorController::Setting::NoType:
            return jsUndefined();
        case InspectorController::Setting::StringType:
            return jsString(exec, setting.string());
        case InspectorController::Setting::DoubleType:
            return jsNumber(exec, setting.doubleValue());
        case InspectorController::Setting::IntegerType:
            return jsNumber(exec, setting.integerValue());
        case InspectorController::Setting::BooleanType:
            return jsBoolean(setting.booleanValue());
        case InspectorController::Setting::StringVectorType: {
            MarkedArgumentBuffer stringsArray;
            const Vector<String>& strings = setting.stringVector();
            const unsigned length = strings.size();
            for (unsigned i = 0; i < length; ++i)
                stringsArray.append(jsString(exec, strings[i]));
            return constructArray(exec, stringsArray);
        }
    }
}

JSValue JSInspectorController::setSetting(ExecState* exec, const ArgList& args)
{
    if (args.size() < 2)
        return jsUndefined();

    String key = args.at(0).toString(exec);
    if (exec->hadException())
        return jsUndefined();

    InspectorController::Setting setting;

    JSValue value = args.at(1);
    if (value.isUndefined() || value.isNull()) {
        // Do nothing. The setting is already NoType.
        ASSERT(setting.type() == InspectorController::Setting::NoType);
    } else if (value.isString())
        setting.set(value.toString(exec));
    else if (value.isNumber())
        setting.set(value.toNumber(exec));
    else if (value.isBoolean())
        setting.set(value.toBoolean(exec));
    else {
        JSArray* jsArray = asArray(value);
        if (!jsArray)
            return jsUndefined();
        Vector<String> strings;
        for (unsigned i = 0; i < jsArray->length(); ++i) {
            String item = jsArray->get(exec, i).toString(exec);
            if (exec->hadException())
                return jsUndefined();
            strings.append(item);
        }
        setting.set(strings);
    }

    if (exec->hadException())
        return jsUndefined();

    impl()->setSetting(key, setting);

    return jsUndefined();
}

JSValue JSInspectorController::wrapCallback(ExecState* exec, const ArgList& args)
{
    if (args.size() < 1)
        return jsUndefined();

    return JSInspectorCallbackWrapper::wrap(exec, args.at(0));
}

#if ENABLE(JAVASCRIPT_DEBUGGER)

JSValue JSInspectorController::currentCallFrame(ExecState* exec, const ArgList&)
{
    JavaScriptCallFrame* callFrame = impl()->currentCallFrame();
    if (!callFrame || !callFrame->isValid())
        return jsUndefined();

    // FIXME: I am not sure if this is actually needed. Can we just use exec?
    ExecState* globalExec = callFrame->scopeChain()->globalObject()->globalExec();

    JSLock lock(false);
    return JSInspectedObjectWrapper::wrap(globalExec, toJS(exec, callFrame));
}

JSValue JSInspectorController::profiles(JSC::ExecState* exec, const JSC::ArgList&)
{
    JSLock lock(false);
    MarkedArgumentBuffer result;
    const Vector<RefPtr<Profile> >& profiles = impl()->profiles();

    for (size_t i = 0; i < profiles.size(); ++i)
        result.append(toJS(exec, profiles[i].get()));

    return constructArray(exec, result);
}

#endif

} // namespace WebCore
