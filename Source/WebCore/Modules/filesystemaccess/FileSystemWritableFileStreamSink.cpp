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
#include "FileSystemWritableFileStreamSink.h"

#include "FileSystemFileHandle.h"
#include "FileSystemWritableFileStream.h"
#include "FileSystemWriteCloseReason.h"
#include "JSBlob.h"
#include <JavaScriptCore/ArrayBufferView.h>
#include <JavaScriptCore/JSArrayBuffer.h>
#include <JavaScriptCore/JSArrayBufferView.h>
#include <JavaScriptCore/JSString.h>

namespace WebCore {

static FileSystemWritableFileStream::WriteParams writeParamsFromChunk(FileSystemWritableFileStream::ChunkType&& chunk)
{
    FileSystemWritableFileStream::WriteParams result;
    result.type = FileSystemWritableFileStream::WriteCommandType::Write;
    WTF::switchOn(WTFMove(chunk), [&](FileSystemWritableFileStream::WriteParams&& params) {
        result = WTFMove(params);
    }, [&](RefPtr<JSC::ArrayBufferView>&& data) {
        result.data = WTFMove(data);
    }, [&](RefPtr<JSC::ArrayBuffer>&& data) {
        result.data = WTFMove(data);
    }, [&](RefPtr<Blob>&& data) {
        result.data = WTFMove(data);
    }, [&](String&& data) {
        result.data = WTFMove(data);
    });

    return result;
}

static void fetchDataBytesForWrite(const FileSystemWritableFileStream::DataVariant& data, CompletionHandler<void(bool, std::span<const uint8_t>)>&& completionHandler)
{
    WTF::switchOn(data, [&](const RefPtr<JSC::ArrayBufferView>& bufferView) {
        if (!bufferView || bufferView->isDetached())
            return completionHandler(false, { });

        RefPtr buffer = bufferView->possiblySharedBuffer();
        if (!buffer)
            return completionHandler(false, { });

        completionHandler(true, buffer->span());
    }, [&](const RefPtr<JSC::ArrayBuffer>& buffer) {
        if (!buffer || buffer->isDetached())
            return completionHandler(false, { });

        completionHandler(true, buffer->span());
    }, [&](const RefPtr<Blob>& blob) {
        if (!blob)
            return completionHandler(false, { });

        // FIXME: For optimization, we may just send blob URL to backend and let it fetch data instead of fetching data here.
        blob->getArrayBuffer([completionHandler = WTFMove(completionHandler)](auto&& result) mutable {
            if (result.hasException())
                return completionHandler(false, { });

            Ref buffer = result.releaseReturnValue();
            if (buffer->isDetached())
                return completionHandler(false, { });

            completionHandler(true, buffer->span());
        });
    }, [&](const String& string) {
        completionHandler(true, string.utf8().span());
    });
}

ExceptionOr<Ref<FileSystemWritableFileStreamSink>> FileSystemWritableFileStreamSink::create(FileSystemFileHandle& source)
{
    return adoptRef(*new FileSystemWritableFileStreamSink(source));
}

FileSystemWritableFileStreamSink::FileSystemWritableFileStreamSink(FileSystemFileHandle& source)
    : m_source(source)
{
}

FileSystemWritableFileStreamSink::~FileSystemWritableFileStreamSink()
{
    if (!m_isClosed)
        protectedSource()->closeWritable(FileSystemWriteCloseReason::Completed);
}

// https://fs.spec.whatwg.org/#write-a-chunk
void FileSystemWritableFileStreamSink::write(ScriptExecutionContext& context, JSC::JSValue value, DOMPromiseDeferred<void>&& promise)
{
    ASSERT(!m_isClosed);

    auto scope = DECLARE_THROW_SCOPE(context.vm());
    auto chunkResult = convert<IDLUnion<IDLArrayBufferView, IDLArrayBuffer, IDLInterface<Blob>, IDLUSVString, IDLDictionary<FileSystemWritableFileStream::WriteParams>>>(*context.globalObject(), value);
    if (UNLIKELY(chunkResult.hasException(scope))) {
        scope.clearException();
        return protectedSource()->executeCommandForWritable(FileSystemWriteCommandType::Write, std::nullopt, std::nullopt, { }, true, WTFMove(promise));
    }

    auto writeParams = writeParamsFromChunk(chunkResult.releaseReturnValue());
    switch (writeParams.type) {
    case FileSystemWriteCommandType::Seek:
    case FileSystemWriteCommandType::Truncate:
        return protectedSource()->executeCommandForWritable(writeParams.type, writeParams.position, writeParams.size, { }, false, WTFMove(promise));
    case FileSystemWriteCommandType::Write: {
        if (!writeParams.data)
            return protectedSource()->executeCommandForWritable(writeParams.type, writeParams.position, writeParams.size, { }, true, WTFMove(promise));

        fetchDataBytesForWrite(*writeParams.data, [this, protectedThis = Ref { *this }, promise = WTFMove(promise), type = writeParams.type, size = writeParams.size, position = writeParams.position](bool fetched, auto dataBytes) mutable {
            protectedSource()->executeCommandForWritable(type, position, size, dataBytes, !fetched, WTFMove(promise));
        });
    }
    }
}

void FileSystemWritableFileStreamSink::close()
{
    ASSERT(!m_isClosed);

    m_isClosed = true;
    protectedSource()->closeWritable(FileSystemWriteCloseReason::Completed);
}

void FileSystemWritableFileStreamSink::error(String&&)
{
    ASSERT(!m_isClosed);

    m_isClosed = true;
    protectedSource()->closeWritable(FileSystemWriteCloseReason::Aborted);
}

} // namespace WebCore
