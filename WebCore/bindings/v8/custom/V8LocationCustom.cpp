/*
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
#include "V8Location.h"

#include "CSSHelper.h"
#include "Document.h"
#include "Frame.h"
#include "FrameLoader.h"
#include "KURL.h"
#include "Location.h"
#include "PlatformString.h"
#include "ScriptController.h"
#include "V8Binding.h"
#include "V8BindingState.h"
#include "V8CustomEventListener.h"
#include "V8DOMWindow.h"
#include "V8Location.h"
#include "V8Utilities.h"
#include "V8Proxy.h"

namespace WebCore {

// Notes about V8/JSC porting of this file.
// This class is not very JS-engine specific.  If we can move a couple of
// methods to the scriptController, we should be able to unify the code
// between JSC and V8:
//    toCallingFrame()   - in JSC, this needs an ExecState.
//    isSafeScript()
// Since JSC and V8 have different mechanisms for getting at the calling frame,
// we're just making all these custom for now.  The functionality is simple
// and mirrors JSLocationCustom.cpp.

void V8Location::hashAccessorSetter(v8::Local<v8::String> name, v8::Local<v8::Value> value, const v8::AccessorInfo& info)
{
    INC_STATS("DOM.Location.hash._set");
    v8::Handle<v8::Object> holder = info.Holder();
    Location* imp = V8Location::toNative(holder);
    String hash = toWebCoreString(value);

    Frame* frame = imp->frame();
    if (!frame)
        return;

    KURL url = frame->loader()->url();
    String oldRef = url.fragmentIdentifier();

    if (hash.startsWith("#"))
        hash = hash.substring(1);

    // Note that by parsing the URL and *then* comparing fragments, we are
    // comparing fragments post-canonicalization, and so this handles the
    // cases where fragment identifiers are ignored or invalid.
    url.setFragmentIdentifier(hash);
    String newRef = url.fragmentIdentifier();
    if (oldRef == newRef || (oldRef.isNull() && newRef.isEmpty()))
        return;

    navigateIfAllowed(frame, url, false, false);
}

void V8Location::hostAccessorSetter(v8::Local<v8::String> name, v8::Local<v8::Value> value, const v8::AccessorInfo& info)
{
    INC_STATS("DOM.Location.host._set");
    v8::Handle<v8::Object> holder = info.Holder();
    Location* imp = V8Location::toNative(holder);
    String host = toWebCoreString(value);

    Frame* frame = imp->frame();
    if (!frame)
        return;

    KURL url = frame->loader()->url();
    String newHost = host.left(host.find(":"));
    String newPort = host.substring(host.find(":") + 1);
    url.setHost(newHost);
    url.setPort(newPort.toUInt());

    navigateIfAllowed(frame, url, false, false);
}

void V8Location::hostnameAccessorSetter(v8::Local<v8::String> name, v8::Local<v8::Value> value, const v8::AccessorInfo& info)
{
    INC_STATS("DOM.Location.hostname._set");
    v8::Handle<v8::Object> holder = info.Holder();
    Location* imp = V8Location::toNative(holder);
    String hostname = toWebCoreString(value);

    Frame* frame = imp->frame();
    if (!frame)
        return;

    KURL url = frame->loader()->url();
    url.setHost(hostname);

    navigateIfAllowed(frame, url, false, false);
}

void V8Location::hrefAccessorSetter(v8::Local<v8::String> name, v8::Local<v8::Value> value, const v8::AccessorInfo& info)
{
    INC_STATS("DOM.Location.href._set");
    v8::Handle<v8::Object> holder = info.Holder();
    Location* imp = V8Location::toNative(holder);

    Frame* frame = imp->frame();
    if (!frame)
        return;

    KURL url = completeURL(toWebCoreString(value));
    if (url.isNull())
        return;

    if (!shouldAllowNavigation(frame))
        return;

    navigateIfAllowed(frame, url, false, false);
}

void V8Location::pathnameAccessorSetter(v8::Local<v8::String> name, v8::Local<v8::Value> value, const v8::AccessorInfo& info)
{
    INC_STATS("DOM.Location.pathname._set");
    v8::Handle<v8::Object> holder = info.Holder();
    Location* imp = V8Location::toNative(holder);
    String pathname = toWebCoreString(value);

    Frame* frame = imp->frame();
    if (!frame)
        return;

    KURL url = frame->loader()->url();
    url.setPath(pathname);

    navigateIfAllowed(frame, url, false, false);
}

void V8Location::portAccessorSetter(v8::Local<v8::String> name, v8::Local<v8::Value> value, const v8::AccessorInfo& info)
{
    INC_STATS("DOM.Location.port._set");
    v8::Handle<v8::Object> holder = info.Holder();
    Location* imp = V8Location::toNative(holder);
    String port = toWebCoreString(value);

    Frame* frame = imp->frame();
    if (!frame)
        return;

    KURL url = frame->loader()->url();
    url.setPort(port.toUInt());

    navigateIfAllowed(frame, url, false, false);
}

void V8Location::protocolAccessorSetter(v8::Local<v8::String> name, v8::Local<v8::Value> value, const v8::AccessorInfo& info)
{
    INC_STATS("DOM.Location.protocol._set");
    v8::Handle<v8::Object> holder = info.Holder();
    Location* imp = V8Location::toNative(holder);
    String protocol = toWebCoreString(value);

    Frame* frame = imp->frame();
    if (!frame)
        return;

    KURL url = frame->loader()->url();
    if (!url.setProtocol(protocol)) {
        throwError("Can't set protocol", V8Proxy::SyntaxError);
        return;
    }

    navigateIfAllowed(frame, url, false, false);
}

void V8Location::searchAccessorSetter(v8::Local<v8::String> name, v8::Local<v8::Value> value, const v8::AccessorInfo& info)
{
    INC_STATS("DOM.Location.search._set");
    v8::Handle<v8::Object> holder = info.Holder();
    Location* imp = V8Location::toNative(holder);
    String query = toWebCoreString(value);

    Frame* frame = imp->frame();
    if (!frame)
        return;

    KURL url = frame->loader()->url();
    url.setQuery(query);

    navigateIfAllowed(frame, url, false, false);
}

v8::Handle<v8::Value> V8Location::reloadAccessorGetter(v8::Local<v8::String> name, const v8::AccessorInfo& info)
{
    INC_STATS("DOM.Location.reload._get");
    static v8::Persistent<v8::FunctionTemplate> privateTemplate = v8::Persistent<v8::FunctionTemplate>::New(v8::FunctionTemplate::New(V8Location::reloadCallback, v8::Handle<v8::Value>(), v8::Signature::New(V8Location::GetRawTemplate())));
    v8::Handle<v8::Object> holder = V8DOMWrapper::lookupDOMWrapper(V8Location::GetTemplate(), info.This());
    if (holder.IsEmpty()) {
        // can only reach here by 'object.__proto__.func', and it should passed
        // domain security check already
        return privateTemplate->GetFunction();
    }
    Location* imp = V8Location::toNative(holder);
    if (!V8BindingSecurity::canAccessFrame(V8BindingState::Only(), imp->frame(), false)) {
        static v8::Persistent<v8::FunctionTemplate> sharedTemplate = v8::Persistent<v8::FunctionTemplate>::New(v8::FunctionTemplate::New(V8Location::reloadCallback, v8::Handle<v8::Value>(), v8::Signature::New(V8Location::GetRawTemplate())));
        return sharedTemplate->GetFunction();
    }
    return privateTemplate->GetFunction();
}

v8::Handle<v8::Value> V8Location::replaceAccessorGetter(v8::Local<v8::String> name, const v8::AccessorInfo& info)
{
    INC_STATS("DOM.Location.replace._get");
    static v8::Persistent<v8::FunctionTemplate> privateTemplate = v8::Persistent<v8::FunctionTemplate>::New(v8::FunctionTemplate::New(V8Location::replaceCallback, v8::Handle<v8::Value>(), v8::Signature::New(V8Location::GetRawTemplate())));
    v8::Handle<v8::Object> holder = V8DOMWrapper::lookupDOMWrapper(V8Location::GetTemplate(), info.This());
    if (holder.IsEmpty()) {
        // can only reach here by 'object.__proto__.func', and it should passed
        // domain security check already
        return privateTemplate->GetFunction();
    }
    Location* imp = V8Location::toNative(holder);
    if (!V8BindingSecurity::canAccessFrame(V8BindingState::Only(), imp->frame(), false)) {
        static v8::Persistent<v8::FunctionTemplate> sharedTemplate = v8::Persistent<v8::FunctionTemplate>::New(v8::FunctionTemplate::New(V8Location::replaceCallback, v8::Handle<v8::Value>(), v8::Signature::New(V8Location::GetRawTemplate())));
        return sharedTemplate->GetFunction();
    }
    return privateTemplate->GetFunction();
}

v8::Handle<v8::Value> V8Location::assignAccessorGetter(v8::Local<v8::String> name, const v8::AccessorInfo& info)
{
    INC_STATS("DOM.Location.assign._get");
    static v8::Persistent<v8::FunctionTemplate> privateTemplate =
        v8::Persistent<v8::FunctionTemplate>::New(v8::FunctionTemplate::New(V8Location::assignCallback, v8::Handle<v8::Value>(), v8::Signature::New(V8Location::GetRawTemplate())));
    v8::Handle<v8::Object> holder = V8DOMWrapper::lookupDOMWrapper(V8Location::GetTemplate(), info.This());
    if (holder.IsEmpty()) {
        // can only reach here by 'object.__proto__.func', and it should passed
        // domain security check already
        return privateTemplate->GetFunction();
    }
    Location* imp = V8Location::toNative(holder);
    if (!V8BindingSecurity::canAccessFrame(V8BindingState::Only(), imp->frame(), false)) {
        static v8::Persistent<v8::FunctionTemplate> sharedTemplate = v8::Persistent<v8::FunctionTemplate>::New(v8::FunctionTemplate::New(V8Location::assignCallback, v8::Handle<v8::Value>(), v8::Signature::New(V8Location::GetRawTemplate())));
        return sharedTemplate->GetFunction();
    }
    return privateTemplate->GetFunction();
}

v8::Handle<v8::Value> V8Location::reloadCallback(const v8::Arguments& args)
{
    // FIXME: we ignore the "forceget" parameter.

    INC_STATS("DOM.Location.reload");
    v8::Handle<v8::Object> holder = args.Holder();
    Location* imp = V8Location::toNative(holder);

    Frame* frame = imp->frame();
    if (!frame || !ScriptController::isSafeScript(frame))
        return v8::Undefined();

    if (!protocolIsJavaScript(frame->loader()->url()))
        frame->redirectScheduler()->scheduleRefresh(processingUserGesture());
    return v8::Undefined();
}

v8::Handle<v8::Value> V8Location::replaceCallback(const v8::Arguments& args)
{
    INC_STATS("DOM.Location.replace");
    v8::Handle<v8::Object> holder = args.Holder();
    Location* imp = V8Location::toNative(holder);

    Frame* frame = imp->frame();
    if (!frame)
        return v8::Undefined();

    KURL url = completeURL(toWebCoreString(args[0]));
    if (url.isNull())
        return v8::Undefined();

    if (!shouldAllowNavigation(frame))
        return v8::Undefined();

    navigateIfAllowed(frame, url, true, true);
    return v8::Undefined();
}

v8::Handle<v8::Value> V8Location::assignCallback(const v8::Arguments& args)
{
    INC_STATS("DOM.Location.assign");
    v8::Handle<v8::Object> holder = args.Holder();
    Location* imp = V8Location::toNative(holder);

    Frame* frame = imp->frame();
    if (!frame)
        return v8::Undefined();

    KURL url = completeURL(toWebCoreString(args[0]));
    if (url.isNull())
        return v8::Undefined();

    if (!shouldAllowNavigation(frame))
        return v8::Undefined();

    navigateIfAllowed(frame, url, false, false);
    return v8::Undefined();
}

v8::Handle<v8::Value> V8Location::valueOfCallback(const v8::Arguments& args)
{
    // Just return the this object the way the normal valueOf function
    // on the Object prototype would.  The valueOf function is only
    // added to make sure that it cannot be overwritten on location
    // objects, since that would provide a hook to change the string
    // conversion behavior of location objects.
    return args.This();
}

v8::Handle<v8::Value> V8Location::toStringCallback(const v8::Arguments& args)
{
    INC_STATS("DOM.Location.toString");
    v8::Handle<v8::Object> holder = args.Holder();
    Location* imp = V8Location::toNative(holder);
    if (!V8BindingSecurity::canAccessFrame(V8BindingState::Only(), imp->frame(), true))
        return v8::Undefined();
    String result = imp->href();
    return v8String(result);
}

bool V8Location::indexedSecurityCheck(v8::Local<v8::Object> host, uint32_t index, v8::AccessType type, v8::Local<v8::Value>)
{
    // Only allow same origin access
    Location* imp =  V8Location::toNative(host);
    return V8BindingSecurity::canAccessFrame(V8BindingState::Only(), imp->frame(), false);
}

bool V8Location::namedSecurityCheck(v8::Local<v8::Object> host, v8::Local<v8::Value> key, v8::AccessType type, v8::Local<v8::Value>)
{
    // Only allow same origin access
    Location* imp = V8Location::toNative(host);
    return V8BindingSecurity::canAccessFrame(V8BindingState::Only(), imp->frame(), false);
}

v8::Handle<v8::Value> toV8(Location* impl)
{
    if (!impl)
        return v8::Null();
    v8::Handle<v8::Object> wrapper = getDOMObjectMap().get(impl);
    if (wrapper.IsEmpty()) {
        wrapper = V8Location::wrap(impl);
        if (!wrapper.IsEmpty())
            V8DOMWrapper::setHiddenWindowReference(impl->frame(), wrapper);
    }
    return wrapper;
}

}  // namespace WebCore

