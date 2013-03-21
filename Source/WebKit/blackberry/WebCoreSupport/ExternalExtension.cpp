/*
 * Copyright (C) 2012 Research In Motion Limited. All rights reserved.
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
#include "ExternalExtension.h"

#include "Chrome.h"
#include "ChromeClientBlackBerry.h"
#include "Frame.h"
#include "Page.h"
#include "SecurityOrigin.h"
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

static JSValueRef addSearchProviderMethod(
    JSContextRef ctx, JSObjectRef functionObject, JSObjectRef thisObject,
    size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
    JSValueRef jsRetVal = JSValueMakeUndefined(ctx);
    if (argumentCount < 1)
        return jsRetVal;

    // Double check if page chrome client exists
    Frame* frame = reinterpret_cast<Frame*>(JSObjectGetPrivate(thisObject));
    if (frame) {
        Page* page = frame->page();
        if (!page || !page->chrome())
            return jsRetVal;

        JSStringRef string = JSValueToStringCopy(ctx, arguments[0], 0);
        size_t sizeUTF8 = JSStringGetMaximumUTF8CStringSize(string);
        char* newURL = new char[sizeUTF8];
        JSStringGetUTF8CString(string, newURL, sizeUTF8);
        JSStringRelease(string);

        String originURL = frame->document()->securityOrigin()->toString();
        ChromeClientBlackBerry* chrome = static_cast<ChromeClientBlackBerry*> (page->chrome()->client());
        chrome->addSearchProvider(originURL, newURL);
    }

    return jsRetVal;
}

static JSValueRef IsSearchProviderInstalledMethod(
    JSContextRef ctx, JSObjectRef functionObject, JSObjectRef thisObject,
    size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
    JSValueRef jsRetVal = JSValueMakeUndefined(ctx);
    if (argumentCount < 1)
        return jsRetVal;

    // Double check if page chrome client exists
    Frame* frame = reinterpret_cast<Frame*>(JSObjectGetPrivate(thisObject));
    if (frame) {
        Page* page = frame->page();
        if (!page || !page->chrome())
            return jsRetVal;

        JSStringRef string = JSValueToStringCopy(ctx, arguments[0], 0);
        size_t sizeUTF8 = JSStringGetMaximumUTF8CStringSize(string);
        char* newURL = new char[sizeUTF8];
        JSStringGetUTF8CString(string, newURL, sizeUTF8);
        JSStringRelease(string);

        ChromeClientBlackBerry* chrome = static_cast<ChromeClientBlackBerry*> (page->chrome()->client());
        int retVal = chrome->isSearchProviderInstalled(newURL);

        jsRetVal = JSValueMakeNumber(ctx, retVal);
    }

    return jsRetVal;
}

static void externalExtensionInitialize(JSContextRef context, JSObjectRef object)
{
    UNUSED_PARAM(context);
    UNUSED_PARAM(object);
}

static void externalExtensionFinalize(JSObjectRef object)
{
    UNUSED_PARAM(object);
}

static JSStaticFunction externalExtensionStaticFunctions[] = {
    { "AddSearchProvider", addSearchProviderMethod, kJSPropertyAttributeNone },
    { "IsSearchProviderInstalled", IsSearchProviderInstalledMethod, kJSPropertyAttributeReadOnly },
    { 0, 0, 0 }
};

static JSStaticValue externalExtensionStaticValues[] = {
    { 0, 0, 0, 0 }
};

// FIXME: Revisit the creation of this class and make sure this is the best way to approach it.
void attachExternalExtensionObjectToFrame(Frame* frame)
{
    JSDOMWindow* window = frame->script()->windowShell(mainThreadNormalWorld())->window();

    JSC::ExecState* exec = window->globalExec();
    JSC::JSLockHolder lock(exec);

    JSContextRef scriptCtx = toRef(exec);

    JSClassDefinition definition = kJSClassDefinitionEmpty;
    definition.staticValues = externalExtensionStaticValues;
    definition.staticFunctions = externalExtensionStaticFunctions;
    definition.initialize = externalExtensionInitialize;
    definition.finalize = externalExtensionFinalize;
    JSClassRef clientClass = JSClassCreate(&definition);

    JSObjectRef clientClassObject = JSObjectMake(scriptCtx, clientClass, 0);
    JSObjectSetPrivate(clientClassObject, reinterpret_cast<void*>(frame));

    String name("external");

    JSC::PutPropertySlot slot;
    window->put(window, exec, JSC::Identifier(exec, name), toJS(clientClassObject), slot);

    JSClassRelease(clientClass);
}
