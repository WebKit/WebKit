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

#pragma once

#include <JavaScriptCore/ArrayBufferView.h>
#include <WebCore/Blob.h>
#include <WebCore/FileSystemWriteCommandType.h>
#include <WebCore/WritableStream.h>

namespace WebCore {

template<typename> class DOMPromiseDeferred;

class FileSystemWritableFileStream : public WritableStream {
public:
    static ExceptionOr<Ref<FileSystemWritableFileStream>> create(JSDOMGlobalObject&, Ref<WritableStreamSink>&&);

    using WriteCommandType = FileSystemWriteCommandType;
    using DataVariant = Variant<RefPtr<JSC::ArrayBufferView>, RefPtr<JSC::ArrayBuffer>, RefPtr<Blob>, String>;
    struct WriteParams {
        WriteCommandType type;
        std::optional<uint64_t> size { };
        std::optional<uint64_t> position { };
        std::optional<std::optional<DataVariant>> data { };
    };

    using ChunkType = Variant<RefPtr<JSC::ArrayBufferView>, RefPtr<JSC::ArrayBuffer>, RefPtr<Blob>, String, WriteParams>;
    void write(JSC::JSGlobalObject&, const ChunkType&, DOMPromiseDeferred<void>&&);
    void seek(JSC::JSGlobalObject&, uint64_t position, DOMPromiseDeferred<void>&&);
    void truncate(JSC::JSGlobalObject&, uint64_t size, DOMPromiseDeferred<void>&&);
    WritableStream::Type type() const final { return WritableStream::Type::FileSystem; }

private:
    explicit FileSystemWritableFileStream(Ref<InternalWritableStream>&&);
};

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_BEGIN(WebCore::FileSystemWritableFileStream)
    static bool isType(const WebCore::WritableStream& stream) { return stream.type() == WebCore::WritableStream::Type::FileSystem; }
SPECIALIZE_TYPE_TRAITS_END()
