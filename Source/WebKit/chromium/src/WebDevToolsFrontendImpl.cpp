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

#include "ContextMenuController.h"
#include "ContextMenuItem.h"
#include "DOMWindow.h"
#include "Document.h"
#include "Event.h"
#include "Frame.h"
#include "InspectorController.h"
#include "InspectorFrontendClientImpl.h"
#include "InspectorFrontendHost.h"
#include "Node.h"
#include "Page.h"
#include "Pasteboard.h"
#include "PlatformString.h"
#include "ScriptController.h"
#include "SecurityOrigin.h"
#include "Settings.h"
#include "V8Binding.h"
#include "V8DOMWrapper.h"
#include "V8InspectorFrontendHost.h"
#include "V8MouseEvent.h"
#include "V8Node.h"
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
{
    InspectorController* ic = m_webViewImpl->page()->inspectorController();
    ic->setInspectorFrontendClient(adoptPtr(new InspectorFrontendClientImpl(m_webViewImpl->page(), m_client, this)));

    // Put each DevTools frontend Page into its own (single page) group so that it's not
    // deferred along with the inspected page.
    m_webViewImpl->page()->setGroupName(String());
}

WebDevToolsFrontendImpl::~WebDevToolsFrontendImpl()
{
}

void WebDevToolsFrontendImpl::dispatchOnInspectorFrontend(const WebString& message)
{
    WebFrameImpl* frame = m_webViewImpl->mainFrameImpl();
    v8::HandleScope scope;
    v8::Handle<v8::Context> frameContext = frame->frame() ? frame->frame()->script()->currentWorldContext() : v8::Local<v8::Context>();
    v8::Context::Scope contextScope(frameContext);
    v8::Handle<v8::Value> inspectorBackendValue = frameContext->Global()->Get(v8::String::New("InspectorBackend"));
    if (!inspectorBackendValue->IsObject())
        return;
    v8::Handle<v8::Object> inspectorBackend = v8::Handle<v8::Object>::Cast(inspectorBackendValue);
    v8::Handle<v8::Value> dispatchFunction = inspectorBackend->Get(v8::String::New("dispatch"));
     // The frame might have navigated away from the front-end page (which is still weird).
    if (!dispatchFunction->IsFunction())
        return;
    v8::Handle<v8::Function> function = v8::Handle<v8::Function>::Cast(dispatchFunction);
    Vector< v8::Handle<v8::Value> > args;
    args.append(ToV8String(message));
    v8::TryCatch tryCatch;
    tryCatch.SetVerbose(true);
    ScriptController::callFunctionWithInstrumentation(frame->frame() ? frame->frame()->document() : 0, function, inspectorBackend, args.size(), args.data());
}

} // namespace WebKit
