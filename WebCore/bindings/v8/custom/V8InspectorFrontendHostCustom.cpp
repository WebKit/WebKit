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
#include "V8InspectorFrontendHost.h"

#include "InspectorController.h"
#include "InspectorFrontendHost.h"

#include "V8Binding.h"
#include "V8MouseEvent.h"
#include "V8Proxy.h"

namespace WebCore {

v8::Handle<v8::Value> V8InspectorFrontendHost::platformCallback(const v8::Arguments&)
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

v8::Handle<v8::Value> V8InspectorFrontendHost::portCallback(const v8::Arguments&)
{
    return v8::Undefined();
}

v8::Handle<v8::Value> V8InspectorFrontendHost::showContextMenuCallback(const v8::Arguments& args)
{
    if (args.Length() < 2)
        return v8::Undefined();

    v8::Local<v8::Object> eventWrapper = v8::Local<v8::Object>::Cast(args[0]);
    if (!V8MouseEvent::info.equals(V8DOMWrapper::domWrapperType(eventWrapper)))
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

    InspectorFrontendHost* frontendHost = V8InspectorFrontendHost::toNative(args.Holder());
    frontendHost->showContextMenu(event, items);

    return v8::Undefined();
}

} // namespace WebCore
