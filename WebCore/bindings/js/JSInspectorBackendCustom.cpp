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
#include "JSInspectorBackend.h"

#if ENABLE(INSPECTOR)

#include "Console.h"
#if ENABLE(DATABASE)
#include "Database.h"
#include "JSDatabase.h"
#endif
#include "ExceptionCode.h"
#include "Frame.h"
#include "FrameLoader.h"
#include "InspectorBackend.h"
#include "InspectorController.h"
#include "InspectorResource.h"
#include "JSDOMWindow.h"
#include "JSInspectedObjectWrapper.h"
#include "JSInspectorCallbackWrapper.h"
#include "JSNode.h"
#include "JSRange.h"
#include "Node.h"
#include "Page.h"
#if ENABLE(DOM_STORAGE)
#include "Storage.h"
#include "JSStorage.h"
#endif
#include "TextIterator.h"
#include "VisiblePosition.h"
#include <runtime/JSArray.h>
#include <runtime/JSLock.h>
#include <wtf/Vector.h>

#if ENABLE(JAVASCRIPT_DEBUGGER)
#include "JavaScriptCallFrame.h"
#include "JavaScriptDebugServer.h"
#include "JSJavaScriptCallFrame.h"
#endif

using namespace JSC;

namespace WebCore {

JSValue JSInspectorBackend::highlightDOMNode(JSC::ExecState* exec, const JSC::ArgList& args)
{
    if (args.size() < 1)
        return jsUndefined();

    impl()->highlight(args.at(0).toInt32(exec));
    return jsUndefined();
}

JSValue JSInspectorBackend::search(ExecState* exec, const ArgList& args)
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
JSValue JSInspectorBackend::databaseForId(ExecState* exec, const ArgList& args)
{
    if (args.size() < 1)
        return jsUndefined();

    InspectorController* ic = impl()->inspectorController();
    if (!ic)
        return jsUndefined();

    Database* database = impl()->databaseForId(args.at(0).toInt32(exec));
    if (!database)
        return jsUndefined();
    // Could use currentWorld(exec) ... but which exec!  The following mixed use of exec & inspectedWindow->globalExec() scares me!
    JSDOMWindow* inspectedWindow = toJSDOMWindow(ic->inspectedPage()->mainFrame(), debuggerWorld());
    return JSInspectedObjectWrapper::wrap(inspectedWindow->globalExec(), toJS(exec, database));
}
#endif

JSValue JSInspectorBackend::inspectedWindow(ExecState*, const ArgList&)
{
    InspectorController* ic = impl()->inspectorController();
    if (!ic)
        return jsUndefined();
    JSDOMWindow* inspectedWindow = toJSDOMWindow(ic->inspectedPage()->mainFrame(), debuggerWorld());
    return JSInspectedObjectWrapper::wrap(inspectedWindow->globalExec(), inspectedWindow);
}

JSValue JSInspectorBackend::setting(ExecState* exec, const ArgList& args)
{
    if (args.size() < 1)
        return jsUndefined();

    String key = args.at(0).toString(exec);
    if (exec->hadException())
        return jsUndefined();

    InspectorController* ic = impl()->inspectorController();
    if (!ic)
        return jsUndefined();
    const InspectorController::Setting& setting = ic->setting(key);

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

JSValue JSInspectorBackend::setSetting(ExecState* exec, const ArgList& args)
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

    InspectorController* ic = impl()->inspectorController();
    if (ic)
        ic->setSetting(key, setting);

    return jsUndefined();
}

JSValue JSInspectorBackend::wrapCallback(ExecState* exec, const ArgList& args)
{
    if (args.size() < 1)
        return jsUndefined();

    return JSInspectorCallbackWrapper::wrap(exec, args.at(0));
}

#if ENABLE(JAVASCRIPT_DEBUGGER)

JSValue JSInspectorBackend::currentCallFrame(ExecState* exec, const ArgList&)
{
    JavaScriptCallFrame* callFrame = impl()->currentCallFrame();
    if (!callFrame || !callFrame->isValid())
        return jsUndefined();

    // FIXME: I am not sure if this is actually needed. Can we just use exec?
    ExecState* globalExec = callFrame->scopeChain()->globalObject->globalExec();

    JSLock lock(SilenceAssertionsOnly);
    return JSInspectedObjectWrapper::wrap(globalExec, toJS(exec, callFrame));
}

#endif

JSValue JSInspectorBackend::nodeForId(ExecState* exec, const ArgList& args)
{
    if (args.size() < 1)
        return jsUndefined();

    Node* node = impl()->nodeForId(args.at(0).toInt32(exec));
    if (!node)
        return jsUndefined();

    InspectorController* ic = impl()->inspectorController();
    if (!ic)
        return jsUndefined();

    JSLock lock(SilenceAssertionsOnly);
    JSDOMWindow* inspectedWindow = toJSDOMWindow(ic->inspectedPage()->mainFrame(), debuggerWorld());
    return JSInspectedObjectWrapper::wrap(inspectedWindow->globalExec(), toJS(exec, deprecatedGlobalObjectForPrototype(inspectedWindow->globalExec()), node));
}

JSValue JSInspectorBackend::wrapObject(ExecState* exec, const ArgList& args)
{
    if (args.size() < 2)
        return jsUndefined();

    return impl()->wrapObject(ScriptValue(args.at(0)), args.at(1).toString(exec)).jsValue();
}

JSValue JSInspectorBackend::unwrapObject(ExecState* exec, const ArgList& args)
{
    if (args.size() < 1)
        return jsUndefined();

    return impl()->unwrapObject(args.at(0).toString(exec)).jsValue();
}

JSValue JSInspectorBackend::pushNodePathToFrontend(ExecState* exec, const ArgList& args)
{
    if (args.size() < 2)
        return jsUndefined();

    JSQuarantinedObjectWrapper* wrapper = JSQuarantinedObjectWrapper::asWrapper(args.at(0));
    if (!wrapper)
        return jsUndefined();

    Node* node = toNode(wrapper->unwrappedObject());
    if (!node)
        return jsUndefined();

    bool selectInUI = args.at(1).toBoolean(exec);
    return jsNumber(exec, impl()->pushNodePathToFrontend(node, selectInUI));
}

#if ENABLE(DATABASE)
JSValue JSInspectorBackend::selectDatabase(ExecState*, const ArgList& args)
{
    if (args.size() < 1)
        return jsUndefined();

    JSQuarantinedObjectWrapper* wrapper = JSQuarantinedObjectWrapper::asWrapper(args.at(0));
    if (!wrapper)
        return jsUndefined();

    Database* database = toDatabase(wrapper->unwrappedObject());
    if (database)
        impl()->selectDatabase(database);
    return jsUndefined();
}
#endif

#if ENABLE(DOM_STORAGE)
JSValue JSInspectorBackend::selectDOMStorage(ExecState*, const ArgList& args)
{
    if (args.size() < 1)
        return jsUndefined();
    InspectorController* ic = impl()->inspectorController();
    if (!ic)
        return jsUndefined();

    JSQuarantinedObjectWrapper* wrapper = JSQuarantinedObjectWrapper::asWrapper(args.at(0));
    if (!wrapper)
        return jsUndefined();

    Storage* storage = toStorage(wrapper->unwrappedObject());
    if (storage)
        impl()->selectDOMStorage(storage);
    return jsUndefined();
}
#endif

} // namespace WebCore

#endif // ENABLE(INSPECTOR)
