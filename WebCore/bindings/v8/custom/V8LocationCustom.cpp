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
#include "Location.h"

#include "V8Binding.h"
#include "V8CustomBinding.h"
#include "V8CustomEventListener.h"
#include "V8Location.h"
#include "V8Utilities.h"
#include "V8Proxy.h"

#include "PlatformString.h"
#include "KURL.h"
#include "Document.h"
#include "FrameLoader.h"
#include "ScriptController.h"
#include "CSSHelper.h"
#include "Frame.h"

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

ACCESSOR_SETTER(LocationHash)
{
    INC_STATS("DOM.Location.hash._set");
    v8::Handle<v8::Object> holder = info.Holder();
    Location* imp = V8DOMWrapper::convertToNativeObject<Location>(V8ClassIndex::LOCATION, holder);
    String hash = toWebCoreString(value);

    Frame* frame = imp->frame();
    if (!frame)
        return;

    KURL url = frame->loader()->url();
    String oldRef = url.ref();

    if (hash.startsWith("#"))
        hash = hash.substring(1);
    if (oldRef == hash || (oldRef.isNull() && hash.isEmpty()))
        return;
    url.setRef(hash);

    navigateIfAllowed(frame, url, false, false);
}

ACCESSOR_SETTER(LocationHost)
{
    INC_STATS("DOM.Location.host._set");
    v8::Handle<v8::Object> holder = info.Holder();
    Location* imp = V8DOMWrapper::convertToNativeObject<Location>(V8ClassIndex::LOCATION, holder);
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

ACCESSOR_SETTER(LocationHostname)
{
    INC_STATS("DOM.Location.hostname._set");
    v8::Handle<v8::Object> holder = info.Holder();
    Location* imp = V8DOMWrapper::convertToNativeObject<Location>(V8ClassIndex::LOCATION, holder);
    String hostname = toWebCoreString(value);

    Frame* frame = imp->frame();
    if (!frame)
        return;

    KURL url = frame->loader()->url();
    url.setHost(hostname);

    navigateIfAllowed(frame, url, false, false);
}

ACCESSOR_SETTER(LocationHref)
{
    INC_STATS("DOM.Location.href._set");
    v8::Handle<v8::Object> holder = info.Holder();
    Location* imp = V8DOMWrapper::convertToNativeObject<Location>(V8ClassIndex::LOCATION, holder);

    Frame* frame = imp->frame();
    if (!frame)
        return;

    if (!shouldAllowNavigation(frame))
        return;

    KURL url = completeURL(toWebCoreString(value));
    if (url.isNull())
        return;

    navigateIfAllowed(frame, url, false, false);
}

ACCESSOR_SETTER(LocationPathname)
{
    INC_STATS("DOM.Location.pathname._set");
    v8::Handle<v8::Object> holder = info.Holder();
    Location* imp = V8DOMWrapper::convertToNativeObject<Location>(V8ClassIndex::LOCATION, holder);
    String pathname = toWebCoreString(value);

    Frame* frame = imp->frame();
    if (!frame)
        return;

    KURL url = frame->loader()->url();
    url.setPath(pathname);

    navigateIfAllowed(frame, url, false, false);
}

ACCESSOR_SETTER(LocationPort)
{
    INC_STATS("DOM.Location.port._set");
    v8::Handle<v8::Object> holder = info.Holder();
    Location* imp = V8DOMWrapper::convertToNativeObject<Location>(V8ClassIndex::LOCATION, holder);
    String port = toWebCoreString(value);

    Frame* frame = imp->frame();
    if (!frame)
        return;

    KURL url = frame->loader()->url();
    url.setPort(port.toUInt());

    navigateIfAllowed(frame, url, false, false);
}

ACCESSOR_SETTER(LocationProtocol)
{
    INC_STATS("DOM.Location.protocol._set");
    v8::Handle<v8::Object> holder = info.Holder();
    Location* imp = V8DOMWrapper::convertToNativeObject<Location>(V8ClassIndex::LOCATION, holder);
    String protocol = toWebCoreString(value);

    Frame* frame = imp->frame();
    if (!frame)
        return;

    KURL url = frame->loader()->url();
    url.setProtocol(protocol);

    navigateIfAllowed(frame, url, false, false);
}

ACCESSOR_SETTER(LocationSearch)
{
    INC_STATS("DOM.Location.search._set");
    v8::Handle<v8::Object> holder = info.Holder();
    Location* imp = V8DOMWrapper::convertToNativeObject<Location>(V8ClassIndex::LOCATION, holder);
    String query = toWebCoreString(value);

    Frame* frame = imp->frame();
    if (!frame)
        return;

    KURL url = frame->loader()->url();
    url.setQuery(query);

    navigateIfAllowed(frame, url, false, false);
}

ACCESSOR_GETTER(LocationReload)
{
    INC_STATS("DOM.Location.reload._get");
    static v8::Persistent<v8::FunctionTemplate> privateTemplate = v8::Persistent<v8::FunctionTemplate>::New(v8::FunctionTemplate::New(v8LocationReloadCallback, v8::Handle<v8::Value>(), v8::Signature::New(V8Location::GetRawTemplate())));
    v8::Handle<v8::Object> holder = V8DOMWrapper::lookupDOMWrapper(V8ClassIndex::LOCATION, info.This());
    if (holder.IsEmpty()) {
        // can only reach here by 'object.__proto__.func', and it should passed
        // domain security check already
        return privateTemplate->GetFunction();
    }
    Location* imp = V8DOMWrapper::convertToNativeObject<Location>(V8ClassIndex::LOCATION, holder);
    if (!V8Proxy::canAccessFrame(imp->frame(), false)) {
        static v8::Persistent<v8::FunctionTemplate> sharedTemplate = v8::Persistent<v8::FunctionTemplate>::New(v8::FunctionTemplate::New(v8LocationReloadCallback, v8::Handle<v8::Value>(), v8::Signature::New(V8Location::GetRawTemplate())));
        return sharedTemplate->GetFunction();
    } else
        return privateTemplate->GetFunction();
}

ACCESSOR_GETTER(LocationReplace)
{
    INC_STATS("DOM.Location.replace._get");
    static v8::Persistent<v8::FunctionTemplate> privateTemplate = v8::Persistent<v8::FunctionTemplate>::New(v8::FunctionTemplate::New(v8LocationReplaceCallback, v8::Handle<v8::Value>(), v8::Signature::New(V8Location::GetRawTemplate())));
    v8::Handle<v8::Object> holder = V8DOMWrapper::lookupDOMWrapper(V8ClassIndex::LOCATION, info.This());
    if (holder.IsEmpty()) {
        // can only reach here by 'object.__proto__.func', and it should passed
        // domain security check already
        return privateTemplate->GetFunction();
    }
    Location* imp = V8DOMWrapper::convertToNativeObject<Location>(V8ClassIndex::LOCATION, holder);
    if (!V8Proxy::canAccessFrame(imp->frame(), false)) {
        static v8::Persistent<v8::FunctionTemplate> sharedTemplate = v8::Persistent<v8::FunctionTemplate>::New(v8::FunctionTemplate::New(v8LocationReplaceCallback, v8::Handle<v8::Value>(), v8::Signature::New(V8Location::GetRawTemplate())));
        return sharedTemplate->GetFunction();
    } else
        return privateTemplate->GetFunction();
}

ACCESSOR_GETTER(LocationAssign)
{
    INC_STATS("DOM.Location.assign._get");
    static v8::Persistent<v8::FunctionTemplate> privateTemplate =
    v8::Persistent<v8::FunctionTemplate>::New(v8::FunctionTemplate::New(v8LocationAssignCallback, v8::Handle<v8::Value>(), v8::Signature::New(V8Location::GetRawTemplate())));
    v8::Handle<v8::Object> holder = V8DOMWrapper::lookupDOMWrapper(V8ClassIndex::LOCATION, info.This());
    if (holder.IsEmpty()) {
        // can only reach here by 'object.__proto__.func', and it should passed
        // domain security check already
        return privateTemplate->GetFunction();
    }
    Location* imp = V8DOMWrapper::convertToNativeObject<Location>(V8ClassIndex::LOCATION, holder);
    if (!V8Proxy::canAccessFrame(imp->frame(), false)) {
        static v8::Persistent<v8::FunctionTemplate> sharedTemplate = v8::Persistent<v8::FunctionTemplate>::New(v8::FunctionTemplate::New(v8LocationAssignCallback, v8::Handle<v8::Value>(), v8::Signature::New(V8Location::GetRawTemplate())));
        return sharedTemplate->GetFunction();
    } else
        return privateTemplate->GetFunction();
}

CALLBACK_FUNC_DECL(LocationReload)
{
    // FIXME: we ignore the "forceget" parameter.

    INC_STATS("DOM.Location.reload");
    v8::Handle<v8::Value> holder = args.Holder();
    Location* imp = V8DOMWrapper::convertToNativeObject<Location>(V8ClassIndex::LOCATION, holder);

    Frame* frame = imp->frame();
    if (!frame || !ScriptController::isSafeScript(frame))
        return v8::Undefined();

    if (!protocolIsJavaScript(frame->loader()->url()))
        frame->loader()->scheduleRefresh(processingUserGesture());
    return v8::Undefined();
}

CALLBACK_FUNC_DECL(LocationReplace)
{
    INC_STATS("DOM.Location.replace");
    v8::Handle<v8::Value> holder = args.Holder();
    Location* imp = V8DOMWrapper::convertToNativeObject<Location>(V8ClassIndex::LOCATION, holder);

    Frame* frame = imp->frame();
    if (!frame)
        return v8::Undefined();

    if (!shouldAllowNavigation(frame))
        return v8::Undefined();

    KURL url = completeURL(toWebCoreString(args[0]));
    if (url.isNull())
        return v8::Undefined();

    navigateIfAllowed(frame, url, true, true);
    return v8::Undefined();
}

CALLBACK_FUNC_DECL(LocationAssign)
{
    INC_STATS("DOM.Location.assign");
    v8::Handle<v8::Value> holder = args.Holder();
    Location* imp = V8DOMWrapper::convertToNativeObject<Location>(V8ClassIndex::LOCATION, holder);

    Frame* frame = imp->frame();
    if (!frame)
        return v8::Undefined();

    if (!shouldAllowNavigation(frame))
        return v8::Undefined();

    KURL url = completeURL(toWebCoreString(args[0]));
    if (url.isNull())
        return v8::Undefined();

    navigateIfAllowed(frame, url, false, false);
    return v8::Undefined();
}

CALLBACK_FUNC_DECL(LocationValueOf)
{
    // Just return the this object the way the normal valueOf function
    // on the Object prototype would.  The valueOf function is only
    // added to make sure that it cannot be overwritten on location
    // objects, since that would provide a hook to change the string
    // conversion behavior of location objects.
    return args.This();
}

CALLBACK_FUNC_DECL(LocationToString)
{
    INC_STATS("DOM.Location.toString");
    v8::Handle<v8::Value> holder = args.Holder();
    Location* imp = V8DOMWrapper::convertToNativeObject<Location>(V8ClassIndex::LOCATION, holder);
    if (!V8Proxy::canAccessFrame(imp->frame(), true))
        return v8::Undefined();
    String result = imp->href();
    return v8String(result);
}

INDEXED_ACCESS_CHECK(Location)
{
    ASSERT(V8ClassIndex::FromInt(data->Int32Value()) == V8ClassIndex::LOCATION);
    // Only allow same origin access
    Location* imp =  V8DOMWrapper::convertToNativeObject<Location>(V8ClassIndex::LOCATION, host);
    return V8Proxy::canAccessFrame(imp->frame(), false);
}

NAMED_ACCESS_CHECK(Location)
{
    ASSERT(V8ClassIndex::FromInt(data->Int32Value()) == V8ClassIndex::LOCATION);
    // Only allow same origin access
    Location* imp = V8DOMWrapper::convertToNativeObject<Location>(V8ClassIndex::LOCATION, host);
    return V8Proxy::canAccessFrame(imp->frame(), false);
}

}  // namespace WebCore

