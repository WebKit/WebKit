/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
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
#include "WebDevToolsFrontendImpl.h"

#include "BoundObject.h"
#include "ContextMenuController.h"
#include "ContextMenuItem.h"
#include "DOMWindow.h"
#include "DebuggerAgent.h"
#include "DevToolsRPCJS.h"
#include "Document.h"
#include "Event.h"
#include "Frame.h"
#include "InspectorBackend.h"
#include "InspectorController.h"
#include "InspectorFrontendHost.h"
#include "Node.h"
#include "Page.h"
#include "Pasteboard.h"
#include "PlatformString.h"
#include "ProfilerAgent.h"
#include "SecurityOrigin.h"
#include "Settings.h"
#include "ToolsAgent.h"
#include "V8Binding.h"
#include "V8DOMWrapper.h"
#include "V8InspectorFrontendHost.h"
#include "V8Node.h"
#include "V8Proxy.h"
#include "V8Utilities.h"
#include "WebDevToolsFrontendClient.h"
#include "WebFrameImpl.h"
#include "WebScriptSource.h"
#include "WebViewImpl.h"
#include <wtf/OwnPtr.h>
#include <wtf/Vector.h>

using namespace WebCore;

namespace WebKit {

static v8::Local<v8::String> ToV8String(const String& s)
{
    if (s.isNull())
        return v8::Local<v8::String>();

    return v8::String::New(reinterpret_cast<const uint16_t*>(s.characters()), s.length());
}

DEFINE_RPC_JS_BOUND_OBJ(DebuggerAgent, DEBUGGER_AGENT_STRUCT, DebuggerAgentDelegate, DEBUGGER_AGENT_DELEGATE_STRUCT)
DEFINE_RPC_JS_BOUND_OBJ(ProfilerAgent, PROFILER_AGENT_STRUCT, ProfilerAgentDelegate, PROFILER_AGENT_DELEGATE_STRUCT)
DEFINE_RPC_JS_BOUND_OBJ(ToolsAgent, TOOLS_AGENT_STRUCT, ToolsAgentDelegate, TOOLS_AGENT_DELEGATE_STRUCT)

WebDevToolsFrontend* WebDevToolsFrontend::create(
    WebView* view,
    WebDevToolsFrontendClient* client,
    const WebString& applicationLocale)
{
    return new WebDevToolsFrontendImpl(
      static_cast<WebViewImpl*>(view),
      client,
      applicationLocale);
}

WebDevToolsFrontendImpl::WebDevToolsFrontendImpl(
    WebViewImpl* webViewImpl,
    WebDevToolsFrontendClient* client,
    const String& applicationLocale)
    : m_webViewImpl(webViewImpl)
    , m_client(client)
    , m_applicationLocale(applicationLocale)
    , m_loaded(false)
{
    WebFrameImpl* frame = m_webViewImpl->mainFrameImpl();
    v8::HandleScope scope;
    v8::Handle<v8::Context> frameContext = V8Proxy::context(frame->frame());

    m_debuggerAgentObj.set(new JSDebuggerAgentBoundObj(this, frameContext, "RemoteDebuggerAgent"));
    m_profilerAgentObj.set(new JSProfilerAgentBoundObj(this, frameContext, "RemoteProfilerAgent"));
    m_toolsAgentObj.set(new JSToolsAgentBoundObj(this, frameContext, "RemoteToolsAgent"));

    // Debugger commands should be sent using special method.
    BoundObject debuggerCommandExecutorObj(frameContext, this, "RemoteDebuggerCommandExecutor");
    debuggerCommandExecutorObj.addProtoFunction(
        "DebuggerCommand",
        WebDevToolsFrontendImpl::jsDebuggerCommand);
    debuggerCommandExecutorObj.addProtoFunction(
        "DebuggerPauseScript",
        WebDevToolsFrontendImpl::jsDebuggerPauseScript);
    debuggerCommandExecutorObj.build();

    BoundObject devToolsHost(frameContext, this, "InspectorFrontendHost");
    devToolsHost.addProtoFunction(
        "loaded",
        WebDevToolsFrontendImpl::jsLoaded);
    devToolsHost.addProtoFunction(
        "platform",
        WebDevToolsFrontendImpl::jsPlatform);
    devToolsHost.addProtoFunction(
        "port",
        WebDevToolsFrontendImpl::jsPort);
    devToolsHost.addProtoFunction(
        "copyText",
        WebDevToolsFrontendImpl::jsCopyText);
    devToolsHost.addProtoFunction(
        "activateWindow",
        WebDevToolsFrontendImpl::jsActivateWindow);
    devToolsHost.addProtoFunction(
        "closeWindow",
        WebDevToolsFrontendImpl::jsCloseWindow);
    devToolsHost.addProtoFunction(
        "attach",
        WebDevToolsFrontendImpl::jsDockWindow);
    devToolsHost.addProtoFunction(
        "detach",
        WebDevToolsFrontendImpl::jsUndockWindow);
    devToolsHost.addProtoFunction(
        "localizedStringsURL",
        WebDevToolsFrontendImpl::jsLocalizedStringsURL);
    devToolsHost.addProtoFunction(
        "hiddenPanels",
        WebDevToolsFrontendImpl::jsHiddenPanels);
    devToolsHost.addProtoFunction(
        "setting",
        WebDevToolsFrontendImpl::jsSetting);
    devToolsHost.addProtoFunction(
        "setSetting",
        WebDevToolsFrontendImpl::jsSetSetting);
    devToolsHost.addProtoFunction(
        "windowUnloading",
        WebDevToolsFrontendImpl::jsWindowUnloading);
    devToolsHost.addProtoFunction(
        "showContextMenu",
        WebDevToolsFrontendImpl::jsShowContextMenu);
    devToolsHost.build();
}

WebDevToolsFrontendImpl::~WebDevToolsFrontendImpl()
{
    if (m_menuProvider)
        m_menuProvider->disconnect();
}

void WebDevToolsFrontendImpl::dispatchMessageFromAgent(const WebDevToolsMessageData& data)
{
    Vector<String> v;
    v.append(data.className);
    v.append(data.methodName);
    for (size_t i = 0; i < data.arguments.size(); i++)
        v.append(data.arguments[i]);
    if (!m_loaded) {
        m_pendingIncomingMessages.append(v);
        return;
    }
    executeScript(v);
}

void WebDevToolsFrontendImpl::executeScript(const Vector<String>& v)
{
    WebFrameImpl* frame = m_webViewImpl->mainFrameImpl();
    v8::HandleScope scope;
    v8::Handle<v8::Context> frameContext = V8Proxy::context(frame->frame());
    v8::Context::Scope contextScope(frameContext);
    v8::Handle<v8::Value> dispatchFunction = frameContext->Global()->Get(v8::String::New("devtools$$dispatch"));
    ASSERT(dispatchFunction->IsFunction());
    v8::Handle<v8::Function> function = v8::Handle<v8::Function>::Cast(dispatchFunction);
    Vector< v8::Handle<v8::Value> > args;
    for (size_t i = 0; i < v.size(); i++)
        args.append(ToV8String(v.at(i)));
    function->Call(frameContext->Global(), args.size(), args.data());
}

void WebDevToolsFrontendImpl::dispatchOnWebInspector(const String& methodName, const String& param)
{
    WebFrameImpl* frame = m_webViewImpl->mainFrameImpl();
    v8::HandleScope scope;
    v8::Handle<v8::Context> frameContext = V8Proxy::context(frame->frame());
    v8::Context::Scope contextScope(frameContext);

    v8::Handle<v8::Value> webInspector = frameContext->Global()->Get(v8::String::New("WebInspector"));
    ASSERT(webInspector->IsObject());
    v8::Handle<v8::Object> webInspectorObj = v8::Handle<v8::Object>::Cast(webInspector);

    v8::Handle<v8::Value> method = webInspectorObj->Get(ToV8String(methodName));
    ASSERT(method->IsFunction());
    v8::Handle<v8::Function> methodFunc = v8::Handle<v8::Function>::Cast(method);
    v8::Handle<v8::Value> args[] = {
      ToV8String(param)
    };
    methodFunc->Call(frameContext->Global(), 1, args);
}

void WebDevToolsFrontendImpl::sendRpcMessage(const WebDevToolsMessageData& data)
{
    m_client->sendMessageToAgent(data);
}

void WebDevToolsFrontendImpl::contextMenuItemSelected(ContextMenuItem* item)
{
    int itemNumber = item->action() - ContextMenuItemBaseCustomTag;
    dispatchOnWebInspector("contextMenuItemSelected", String::number(itemNumber));
}

void WebDevToolsFrontendImpl::contextMenuCleared()
{
    dispatchOnWebInspector("contextMenuCleared", "");
}

v8::Handle<v8::Value> WebDevToolsFrontendImpl::jsLoaded(const v8::Arguments& args)
{
    WebDevToolsFrontendImpl* frontend = static_cast<WebDevToolsFrontendImpl*>(v8::External::Cast(*args.Data())->Value());
    frontend->m_loaded = true;

    // Grant the devtools page the ability to have source view iframes.
    Page* page = V8Proxy::retrieveFrameForEnteredContext()->page();
    SecurityOrigin* origin = page->mainFrame()->domWindow()->securityOrigin();
    origin->grantUniversalAccess();

    for (Vector<Vector<String> >::iterator it = frontend->m_pendingIncomingMessages.begin();
         it != frontend->m_pendingIncomingMessages.end();
         ++it) {
        frontend->executeScript(*it);
    }
    frontend->m_pendingIncomingMessages.clear();
    return v8::Undefined();
}

// static
v8::Handle<v8::Value> WebDevToolsFrontendImpl::jsPlatform(const v8::Arguments& args)
{
#if defined(OS_MACOSX)
    return v8String("mac");
#elif defined(OS_LINUX)
    return v8String("linux");
#elif defined(OS_WIN)
    return v8String("windows");
#else
    return v8String("unknown");
#endif
}

v8::Handle<v8::Value> WebDevToolsFrontendImpl::jsPort(const v8::Arguments& args)
{
    return v8::Undefined();
}

v8::Handle<v8::Value> WebDevToolsFrontendImpl::jsCopyText(const v8::Arguments& args)
{
    String text = WebCore::toWebCoreStringWithNullCheck(args[0]);
    Pasteboard::generalPasteboard()->writePlainText(text);
    return v8::Undefined();
}

v8::Handle<v8::Value> WebDevToolsFrontendImpl::jsActivateWindow(const v8::Arguments& args)
{
    WebDevToolsFrontendImpl* frontend = static_cast<WebDevToolsFrontendImpl*>(v8::External::Cast(*args.Data())->Value());
    frontend->m_client->activateWindow();
    return v8::Undefined();
}

v8::Handle<v8::Value> WebDevToolsFrontendImpl::jsCloseWindow(const v8::Arguments& args)
{
    WebDevToolsFrontendImpl* frontend = static_cast<WebDevToolsFrontendImpl*>(v8::External::Cast(*args.Data())->Value());
    frontend->m_client->closeWindow();
    return v8::Undefined();
}

v8::Handle<v8::Value> WebDevToolsFrontendImpl::jsDockWindow(const v8::Arguments& args)
{
    WebDevToolsFrontendImpl* frontend = static_cast<WebDevToolsFrontendImpl*>(v8::External::Cast(*args.Data())->Value());
    frontend->m_client->dockWindow();
    return v8::Undefined();
}

v8::Handle<v8::Value> WebDevToolsFrontendImpl::jsUndockWindow(const v8::Arguments& args)
{
    WebDevToolsFrontendImpl* frontend = static_cast<WebDevToolsFrontendImpl*>(v8::External::Cast(*args.Data())->Value());
    frontend->m_client->undockWindow();
    return v8::Undefined();
}

v8::Handle<v8::Value> WebDevToolsFrontendImpl::jsLocalizedStringsURL(const v8::Arguments& args)
{
    return v8::Undefined();
}

v8::Handle<v8::Value> WebDevToolsFrontendImpl::jsHiddenPanels(const v8::Arguments& args)
{
    return v8String("");
}

v8::Handle<v8::Value> WebDevToolsFrontendImpl::jsDebuggerCommand(const v8::Arguments& args)
{
    WebDevToolsFrontendImpl* frontend = static_cast<WebDevToolsFrontendImpl*>(v8::External::Cast(*args.Data())->Value());
    WebString command = WebCore::toWebCoreStringWithNullCheck(args[0]);
    frontend->m_client->sendDebuggerCommandToAgent(command);
    return v8::Undefined();
}

v8::Handle<v8::Value> WebDevToolsFrontendImpl::jsSetting(const v8::Arguments& args)
{
    return v8::Undefined();
}

v8::Handle<v8::Value> WebDevToolsFrontendImpl::jsSetSetting(const v8::Arguments& args)
{
    return v8::Undefined();
}

v8::Handle<v8::Value> WebDevToolsFrontendImpl::jsDebuggerPauseScript(const v8::Arguments& args)
{
    WebDevToolsFrontendImpl* frontend = static_cast<WebDevToolsFrontendImpl*>(v8::External::Cast(*args.Data())->Value());
    frontend->m_client->sendDebuggerPauseScript();
    return v8::Undefined();
}

v8::Handle<v8::Value> WebDevToolsFrontendImpl::jsWindowUnloading(const v8::Arguments& args)
{
    // TODO(pfeldman): Implement this.
    return v8::Undefined();
}

v8::Handle<v8::Value> WebDevToolsFrontendImpl::jsShowContextMenu(const v8::Arguments& args)
{
    if (args.Length() < 2)
        return v8::Undefined();

    v8::Local<v8::Object> eventWrapper = v8::Local<v8::Object>::Cast(args[0]);
    if (V8DOMWrapper::domWrapperType(eventWrapper) != V8ClassIndex::MOUSEEVENT)
        return v8::Undefined();

    Event* event = V8Event::toNative(eventWrapper);
    if (!args[1]->IsArray())
        return v8::Undefined();

    v8::Local<v8::Array> array = v8::Local<v8::Array>::Cast(args[1]);
    Vector<ContextMenuItem*> items;

    for (size_t i = 0; i < array->Length(); ++i) {
        v8::Local<v8::Object> item = v8::Local<v8::Object>::Cast(array->Get(v8::Integer::New(i)));
        v8::Local<v8::Value> label = item->Get(v8::String::New("label"));
        v8::Local<v8::Value> id = item->Get(v8::String::New("id"));
        if (label->IsUndefined() || id->IsUndefined()) {
          items.append(new ContextMenuItem(SeparatorType,
                                           ContextMenuItemTagNoAction,
                                           String()));
        } else {
          ContextMenuAction typedId = static_cast<ContextMenuAction>(
              ContextMenuItemBaseCustomTag + id->ToInt32()->Value());
          items.append(new ContextMenuItem(ActionType,
                                           typedId,
                                           toWebCoreStringWithNullCheck(label)));
        }
    }

    WebDevToolsFrontendImpl* frontend = static_cast<WebDevToolsFrontendImpl*>(v8::External::Cast(*args.Data())->Value());

    frontend->m_menuProvider = MenuProvider::create(frontend, items);

    ContextMenuController* menuController = frontend->m_webViewImpl->page()->contextMenuController();
    menuController->showContextMenu(event, frontend->m_menuProvider);

    return v8::Undefined();
}

} // namespace WebKit
