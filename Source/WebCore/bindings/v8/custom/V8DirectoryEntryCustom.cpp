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
#include "V8DirectoryEntry.h"

#if ENABLE(FILE_SYSTEM)

#include "DirectoryEntry.h"
#include "ExceptionCode.h"
#include "V8Binding.h"
#include "V8BindingMacros.h"
#include "V8EntryCallback.h"
#include "V8ErrorCallback.h"
#include "V8Proxy.h"
#include <wtf/RefCounted.h>
#include <wtf/RefPtr.h>

namespace WebCore {

v8::Handle<v8::Value> V8DirectoryEntry::getDirectoryCallback(const v8::Arguments& args)
{
    INC_STATS("DOM.DirectoryEntry.getDirectory");
    DirectoryEntry* imp = V8DirectoryEntry::toNative(args.Holder());

    if (args.Length() < 1)
        return V8Proxy::throwNotEnoughArgumentsError();

    STRING_TO_V8PARAMETER_EXCEPTION_BLOCK(V8Parameter<WithUndefinedOrNullCheck>, path, args[0]);
    if (args.Length() <= 1) {
        imp->getDirectory(path);
        return v8::Handle<v8::Value>();
    }
    RefPtr<WebKitFlags> flags;
    if (!isUndefinedOrNull(args[1]) && args[1]->IsObject()) {
        EXCEPTION_BLOCK(v8::Handle<v8::Object>, object, v8::Handle<v8::Object>::Cast(args[1]));
        flags = WebKitFlags::create();
        v8::Local<v8::Value> v8Create = object->Get(v8::String::New("create"));
        if (!v8Create.IsEmpty() && !isUndefinedOrNull(v8Create)) {
            EXCEPTION_BLOCK(bool, isCreate, v8Create->BooleanValue());
            flags->setCreate(isCreate);
        }
        v8::Local<v8::Value> v8Exclusive = object->Get(v8::String::New("exclusive"));
        if (!v8Exclusive.IsEmpty() && !isUndefinedOrNull(v8Exclusive)) {
            EXCEPTION_BLOCK(bool, isExclusive, v8Exclusive->BooleanValue());
            flags->setExclusive(isExclusive);
        }
    }
    RefPtr<EntryCallback> successCallback;
    if (args.Length() > 2 && !args[2]->IsNull() && !args[2]->IsUndefined()) {
        if (!args[2]->IsObject())
            return throwError(TYPE_MISMATCH_ERR);
        successCallback = V8EntryCallback::create(args[2], getScriptExecutionContext());
    }
    RefPtr<ErrorCallback> errorCallback;
    if (args.Length() > 3 && !args[3]->IsNull() && !args[3]->IsUndefined()) {
        if (!args[3]->IsObject())
            return throwError(TYPE_MISMATCH_ERR);
        errorCallback = V8ErrorCallback::create(args[3], getScriptExecutionContext());
    }
    imp->getDirectory(path, flags, successCallback, errorCallback);
    return v8::Handle<v8::Value>();
}

v8::Handle<v8::Value> V8DirectoryEntry::getFileCallback(const v8::Arguments& args)
{
    INC_STATS("DOM.DirectoryEntry.getFile");
    DirectoryEntry* imp = V8DirectoryEntry::toNative(args.Holder());

    if (args.Length() < 1)
        return V8Proxy::throwNotEnoughArgumentsError();

    STRING_TO_V8PARAMETER_EXCEPTION_BLOCK(V8Parameter<WithUndefinedOrNullCheck>, path, args[0]);
    if (args.Length() <= 1) {
        imp->getFile(path);
        return v8::Handle<v8::Value>();
    }
    RefPtr<WebKitFlags> flags;
    if (!isUndefinedOrNull(args[1]) && args[1]->IsObject()) {
        EXCEPTION_BLOCK(v8::Handle<v8::Object>, object, v8::Handle<v8::Object>::Cast(args[1]));
        flags = WebKitFlags::create();
        v8::Local<v8::Value> v8Create = object->Get(v8::String::New("create"));
        if (!v8Create.IsEmpty() && !isUndefinedOrNull(v8Create)) {
            EXCEPTION_BLOCK(bool, isCreate, v8Create->BooleanValue());
            flags->setCreate(isCreate);
        }
        v8::Local<v8::Value> v8Exclusive = object->Get(v8::String::New("exclusive"));
        if (!v8Exclusive.IsEmpty() && !isUndefinedOrNull(v8Exclusive)) {
            EXCEPTION_BLOCK(bool, isExclusive, v8Exclusive->BooleanValue());
            flags->setExclusive(isExclusive);
        }
    }
    RefPtr<EntryCallback> successCallback;
    if (args.Length() > 2 && !args[2]->IsNull() && !args[2]->IsUndefined()) {
        if (!args[2]->IsObject())
            return throwError(TYPE_MISMATCH_ERR);
        successCallback = V8EntryCallback::create(args[2], getScriptExecutionContext());
    }
    RefPtr<ErrorCallback> errorCallback;
    if (args.Length() > 3 && !args[3]->IsNull() && !args[3]->IsUndefined()) {
        if (!args[3]->IsObject())
            return throwError(TYPE_MISMATCH_ERR);
        errorCallback = V8ErrorCallback::create(args[3], getScriptExecutionContext());
    }
    imp->getFile(path, flags, successCallback, errorCallback);
    return v8::Handle<v8::Value>();
}



} // namespace WebCore

#endif // ENABLE(FILE_SYSTEM)
