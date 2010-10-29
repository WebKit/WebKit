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

#if ENABLE(BLOB)

#include "V8FileReader.h"

#include "ScriptExecutionContext.h"
#include "V8ArrayBuffer.h"
#include "V8Binding.h"

namespace WebCore {

v8::Handle<v8::Value> V8FileReader::constructorCallback(const v8::Arguments& args)
{
    INC_STATS("DOM.FileReader.Constructor");

    if (!args.IsConstructCall())
        return throwError("DOM object constructor cannot be called as a function.", V8Proxy::TypeError);

    // Expect no parameters.
    // Allocate a FileReader object as its internal field.
    ScriptExecutionContext* context = getScriptExecutionContext();
    if (!context)
        return throwError("FileReader constructor's associated context is not available", V8Proxy::ReferenceError);
    RefPtr<FileReader> fileReader = FileReader::create(context);
    V8DOMWrapper::setDOMWrapper(args.Holder(), &info, fileReader.get());

    // Add object to the wrapper map.
    fileReader->ref();
    V8DOMWrapper::setJSWrapperForActiveDOMObject(fileReader.get(), v8::Persistent<v8::Object>::New(args.Holder()));
    return args.Holder();
}

v8::Handle<v8::Value> V8FileReader::resultAccessorGetter(v8::Local<v8::String> name, const v8::AccessorInfo& info)
{
    INC_STATS("DOM.FileReader.result._get");
    v8::Handle<v8::Object> holder = info.Holder();
    FileReader* imp = V8FileReader::toNative(holder);
    if (imp->readType() == FileReaderLoader::ReadAsArrayBuffer)
        return toV8(imp->arrayBufferResult());
    return v8StringOrNull(imp->stringResult());
}

} // namespace WebCore

#endif // ENABLE(BLOB)
