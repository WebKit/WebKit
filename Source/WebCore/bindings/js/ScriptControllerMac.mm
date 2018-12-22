/*
 * Copyright (C) 2004, 2005, 2006, 2007, 2008 Apple Inc. All rights reserved.
 * Copyright (C) 2006 Alexey Proskuryakov (ap@nypop.com)
 * Copyright (C) 2008 Nokia Corporation and/or its subsidiary(-ies)
 * Copyright (C) 2008 Eric Seidel <eric@webkit.org>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#import "config.h"
#import "ScriptController.h"

#import "BridgeJSC.h"
#import "CommonVM.h"
#import "DOMWindow.h"
#import "Frame.h"
#import "FrameLoader.h"
#import "FrameLoaderClient.h"
#import "JSDOMWindow.h"
#import "WebScriptObjectPrivate.h"
#import "Widget.h"
#import "objc_instance.h"
#import "runtime_root.h"
#import <JavaScriptCore/APICast.h>
#import <JavaScriptCore/JSContextInternal.h>
#import <JavaScriptCore/JSLock.h>

#if ENABLE(NETSCAPE_PLUGIN_API)
#import "c_instance.h"
#import "NP_jsobject.h"
#import "npruntime_impl.h"
#endif

@interface NSObject (WebPlugin)
- (id)objectForWebScript;
- (NPObject *)createPluginScriptableObject;
- (RefPtr<JSC::Bindings::Instance>)createPluginBindingsInstance:(Ref<JSC::Bindings::RootObject>&&)rootObject;
@end

using namespace JSC::Bindings;

namespace WebCore {

RefPtr<JSC::Bindings::Instance> ScriptController::createScriptInstanceForWidget(Widget* widget)
{
    NSView* widgetView = widget->platformWidget();
    if (!widgetView)
        return nullptr;

    auto rootObject = createRootObject((__bridge void*)widgetView);

    if ([widgetView respondsToSelector:@selector(createPluginBindingsInstance:)])
        return [widgetView createPluginBindingsInstance:WTFMove(rootObject)];
        
    if ([widgetView respondsToSelector:@selector(objectForWebScript)]) {
        id objectForWebScript = [widgetView objectForWebScript];
        if (!objectForWebScript)
            return nullptr;
        return JSC::Bindings::ObjcInstance::create(objectForWebScript, WTFMove(rootObject));
    }

    if ([widgetView respondsToSelector:@selector(createPluginScriptableObject)]) {
#if !ENABLE(NETSCAPE_PLUGIN_API)
        return nullptr;
#else
        NPObject* npObject = [widgetView createPluginScriptableObject];
        if (!npObject)
            return nullptr;
        auto instance = JSC::Bindings::CInstance::create(npObject, WTFMove(rootObject));
        // -createPluginScriptableObject returns a retained NPObject.  The caller is expected to release it.
        _NPN_ReleaseObject(npObject);
        return WTFMove(instance);
#endif
    }

    return nullptr;
}

WebScriptObject *ScriptController::windowScriptObject()
{
    if (!canExecuteScripts(NotAboutToExecuteScript))
        return nil;

    if (!m_windowScriptObject) {
        JSC::JSLockHolder lock(commonVM());
        JSC::Bindings::RootObject* root = bindingRootObject();
        m_windowScriptObject = [WebScriptObject scriptObjectForJSObject:toRef(&jsWindowProxy(pluginWorld())) originRootObject:root rootObject:root];
    }

    return m_windowScriptObject.get();
}

JSContext *ScriptController::javaScriptContext()
{
#if JSC_OBJC_API_ENABLED
    if (!canExecuteScripts(NotAboutToExecuteScript))
        return 0;
    JSContext *context = [JSContext contextWithJSGlobalContextRef:toGlobalRef(bindingRootObject()->globalObject()->globalExec())];
    return context;
#else
    return 0;
#endif
}

void ScriptController::updatePlatformScriptObjects()
{
    if (m_windowScriptObject) {
        JSC::Bindings::RootObject* root = bindingRootObject();
        [m_windowScriptObject.get() _setOriginRootObject:root andRootObject:root];
    }
}

void ScriptController::disconnectPlatformScriptObjects()
{
    if (m_windowScriptObject)
        disconnectWindowWrapper(m_windowScriptObject.get());
}

}
