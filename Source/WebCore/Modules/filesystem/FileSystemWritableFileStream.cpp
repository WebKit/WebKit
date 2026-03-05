/*
 * Copyright (C) 2024 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "FileSystemWritableFileStream.h"

#include "InternalWritableStream.h"
#include "JSBlob.h"
#include "JSDOMPromise.h"
#include "JSDOMPromiseDeferred.h"
#include "JSFileSystemWritableFileStream.h"
#include <JavaScriptCore/JSPromise.h>

namespace WebCore {

ExceptionOr<Ref<FileSystemWritableFileStream>> FileSystemWritableFileStream::create(JSDOMGlobalObject& globalObject, Ref<WritableStreamSink>&& sink)
{
    auto result = createInternalWritableStream(globalObject, WTF::move(sink));
    if (result.hasException())
        return result.releaseException();

    return adoptRef(*new FileSystemWritableFileStream(result.releaseReturnValue()));
}

FileSystemWritableFileStream::FileSystemWritableFileStream(Ref<InternalWritableStream>&& internalStream)
    : WritableStream(WTF::move(internalStream))
{
}

static JSC::JSValue convertChunk(JSC::JSGlobalObject& lexicalGlobalObject, JSDOMGlobalObject& globalObject, const FileSystemWritableFileStream::ChunkType& data)
{
    return WTF::switchOn(data,
        [&](const Ref<JSC::ArrayBufferView>& arrayBufferView) {
            if (arrayBufferView->isDetached())
                return JSC::jsNull();
            return toJS<IDLArrayBufferView>(lexicalGlobalObject, globalObject, arrayBufferView);
        },
        [&](const Ref<JSC::ArrayBuffer>& arrayBuffer) {
            if (arrayBuffer->isDetached())
                return JSC::jsNull();
            return toJS<IDLArrayBuffer>(lexicalGlobalObject, globalObject, arrayBuffer);
        },
        [&](const Ref<Blob>& blob) {
            return toJS<IDLInterface<Blob>>(lexicalGlobalObject, globalObject, blob);
        },
        [&](const String& string) {
            return toJS<IDLDOMString>(lexicalGlobalObject, string);
        },
        [&](const FileSystemWritableFileStream::WriteParams& params) {
            return toJS<IDLDictionary<FileSystemWritableFileStream::WriteParams>>(lexicalGlobalObject, globalObject, params);
        }
    );
}

void FileSystemWritableFileStream::write(JSC::JSGlobalObject& lexicalGlobalObject, const ChunkType& data, DOMPromiseDeferred<void>&& promise)
{
    auto* globalObject = JSC::jsCast<JSDOMGlobalObject*>(&lexicalGlobalObject);
    RELEASE_ASSERT(globalObject);

    auto jsData = convertChunk(lexicalGlobalObject, *globalObject, data);
    if (jsData == JSC::jsNull())
        return promise.reject(Exception { ExceptionCode::TypeError });

    Ref internalStream = internalWritableStream();
    auto result = internalStream->writeChunkForBingings(lexicalGlobalObject, jsData);
    if (result.hasException())
        return promise.reject(result.releaseException());

    auto* jsPromise = jsCast<JSC::JSPromise*>(result.returnValue());
    if (!jsPromise)
        return promise.reject(Exception { ExceptionCode::UnknownError, "Failed to complete write operation"_s });

    Ref domPromise = DOMPromise::create(*globalObject, *jsPromise);
    domPromise->whenSettledWithResult([promise = WTF::move(promise)](auto*, bool isFulfilled, auto result) mutable {
        if (isFulfilled)
            return promise.resolve();
        return promise.rejectWithCallback([&result](auto&) {
            return result;
        });
    });
}

void FileSystemWritableFileStream::seek(JSC::JSGlobalObject& lexicalGlobalObject, uint64_t position, DOMPromiseDeferred<void>&& promise)
{
    WriteParams params { WriteCommandType::Seek, std::nullopt, position, std::nullopt };
    write(lexicalGlobalObject, params, WTF::move(promise));
}

void FileSystemWritableFileStream::truncate(JSC::JSGlobalObject& lexicalGlobalObject, uint64_t size, DOMPromiseDeferred<void>&& promise)
{
    WriteParams params { WriteCommandType::Truncate, size, std::nullopt, std::nullopt };
    write(lexicalGlobalObject, params, WTF::move(promise));
}

} // namespace WebCore
