/*
 * Copyright (C) 2010, 2011, 2012 Research In Motion Limited. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include "config.h"
#include "ClientExtension.h"

#include "Frame.h"
#include "WebPageClient.h"
#include <JavaScriptCore/API/JSCallbackObject.h>
#include <JavaScriptCore/JSObjectRef.h>
#include <JavaScriptCore/JSStringRef.h>
#include <JavaScriptCore/JSValueRef.h>
#include <JavaScriptCore/JavaScript.h>
#include <string>

using namespace WebCore;
using namespace BlackBerry::WebKit;
using namespace std;

static JSValueRef clientExtensionMethod(
    JSContextRef ctx, JSObjectRef functionObject, JSObjectRef thisObject,
    size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
    JSValueRef jsRetVal = JSValueMakeUndefined(ctx);
    if (argumentCount <= 0)
        return jsRetVal;

    char** strArgs = new char*[argumentCount];
    for (unsigned i = 0; i < argumentCount; ++i) {
        JSStringRef string = JSValueToStringCopy(ctx, arguments[i], 0);
        size_t sizeUTF8 = JSStringGetMaximumUTF8CStringSize(string);
        strArgs[i] = new char[sizeUTF8];
        JSStringGetUTF8CString(string, strArgs[i], sizeUTF8);
        JSStringRelease(string);
    }

    WebPageClient* client = reinterpret_cast<WebPageClient*>(JSObjectGetPrivate(thisObject));
    string retVal = client->invokeClientJavaScriptCallback(strArgs, argumentCount).utf8();
    if (!retVal.empty())
        jsRetVal = JSValueMakeString(ctx, JSStringCreateWithUTF8CString(retVal.c_str()));

    for (unsigned i = 0; i < argumentCount; ++i)
        delete[] strArgs[i];
    delete[] strArgs;

    return jsRetVal;
}

static void clientExtensionInitialize(JSContextRef context, JSObjectRef object)
{
    UNUSED_PARAM(context);
    UNUSED_PARAM(object);
}

static void clientExtensionFinalize(JSObjectRef object)
{
    UNUSED_PARAM(object);
}

static JSStaticFunction clientExtensionStaticFunctions[] = {
    { "callExtensionMethod", clientExtensionMethod, kJSPropertyAttributeNone },
    { 0, 0, 0 }
};

static JSStaticValue clientExtensionStaticValues[] = {
    { 0, 0, 0, 0 }
};

// FIXME: Revisit the creation of this class and make sure this is the best way to approach it.
void attachExtensionObjectToFrame(Frame* frame, WebPageClient* client)
{
    JSC::JSLock lock(JSC::SilenceAssertionsOnly);

    JSDOMWindow* window = frame->script()->windowShell(mainThreadNormalWorld())->window();

    JSC::ExecState* exec = window->globalExec();
    JSContextRef scriptCtx = toRef(exec);

    JSClassDefinition definition = kJSClassDefinitionEmpty;
    definition.staticValues = clientExtensionStaticValues;
    definition.staticFunctions = clientExtensionStaticFunctions;
    definition.initialize = clientExtensionInitialize;
    definition.finalize = clientExtensionFinalize;
    JSClassRef clientClass = JSClassCreate(&definition);

    JSObjectRef clientClassObject = JSObjectMake(scriptCtx, clientClass, 0);
    JSObjectSetPrivate(clientClassObject, reinterpret_cast<void*>(client));

    JSC::UString name("qnx");

    JSC::PutPropertySlot slot;
    window->put(window, exec, JSC::Identifier(exec, name), toJS(clientClassObject), slot);

    JSClassRelease(clientClass);
}
