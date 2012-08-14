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
#include "V8DirectoryEntrySync.h"

#if ENABLE(FILE_SYSTEM)

#include "DirectoryEntrySync.h"
#include "ExceptionCode.h"
#include "V8Binding.h"
#include "V8EntryCallback.h"
#include "V8ErrorCallback.h"
#include "V8FileEntrySync.h"
#include "V8Proxy.h"
#include <wtf/RefCounted.h>
#include <wtf/RefPtr.h>

namespace WebCore {

static bool extractBooleanValue(const v8::Handle<v8::Object>& object, const char* name, ExceptionCode& ec) {
    ec = 0;
    v8::Local<v8::Value> value = object->Get(v8::String::New(name));
    if (!value.IsEmpty() && !isUndefinedOrNull(value)) {
        v8::TryCatch block;
        bool nativeValue = value->BooleanValue();
        if (block.HasCaught()) {
            ec = TYPE_MISMATCH_ERR;
            return false;
        }
        return nativeValue;
    }
    return false;
}

static PassRefPtr<WebKitFlags> getFlags(const v8::Local<v8::Value>& arg, ExceptionCode& ec)
{
    ec = 0;
    if (isUndefinedOrNull(arg) || !arg->IsObject())
        return 0;

    v8::Handle<v8::Object> object;
    {
        v8::TryCatch block;
        object = v8::Handle<v8::Object>::Cast(arg);
        if (block.HasCaught()) {
            ec = TYPE_MISMATCH_ERR;
            return 0;
        }
    }

    bool isCreate = extractBooleanValue(object, "create", ec);
    if (ec)
        return 0;
    bool isExclusive = extractBooleanValue(object, "exclusive", ec);
    if (ec)
        return 0;

    RefPtr<WebKitFlags> flags = WebKitFlags::create();
    flags->setCreate(isCreate);
    flags->setExclusive(isExclusive);

    return flags;
}

v8::Handle<v8::Value> V8DirectoryEntrySync::getDirectoryCallback(const v8::Arguments& args)
{
    INC_STATS("DOM.DirectoryEntrySync.getDirectory");
    DirectoryEntrySync* imp = V8DirectoryEntrySync::toNative(args.Holder());
    ExceptionCode ec = 0;
    STRING_TO_V8PARAMETER_EXCEPTION_BLOCK(V8Parameter<>, path, args[0]);
    RefPtr<WebKitFlags> flags = getFlags(args[1], ec);
    if (UNLIKELY(ec))
        return setDOMException(ec, args.GetIsolate());
    RefPtr<DirectoryEntrySync> result = imp->getDirectory(path, flags, ec);
    if (UNLIKELY(ec))
        return setDOMException(ec, args.GetIsolate());
    return toV8(result.release(), args.GetIsolate());
}

v8::Handle<v8::Value> V8DirectoryEntrySync::getFileCallback(const v8::Arguments& args)
{
    INC_STATS("DOM.DirectoryEntrySync.getFile");
    DirectoryEntrySync* imp = V8DirectoryEntrySync::toNative(args.Holder());
    ExceptionCode ec = 0;
    STRING_TO_V8PARAMETER_EXCEPTION_BLOCK(V8Parameter<>, path, args[0]);
    RefPtr<WebKitFlags> flags = getFlags(args[1], ec);
    if (UNLIKELY(ec))
        return setDOMException(ec, args.GetIsolate());
    RefPtr<FileEntrySync> result = imp->getFile(path, flags, ec);
    if (UNLIKELY(ec))
        return setDOMException(ec, args.GetIsolate());
    return toV8(result.release(), args.GetIsolate());
}



} // namespace WebCore

#endif // ENABLE(FILE_SYSTEM)
