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

#include "Exception.h"
#include "ExceptionOr.h"
#include "FileSystemFileHandle.h"
#include "FileSystemWritableFileStream.h"
#include "FileSystemWriteCloseReason.h"
#include "JSBlob.h"
#include <JavaScriptCore/ArrayBufferView.h>
#include <JavaScriptCore/JSArrayBuffer.h>
#include <JavaScriptCore/JSArrayBufferView.h>
#include <JavaScriptCore/JSString.h>
#include <wtf/CompletionHandler.h>

namespace WebCore {

static ExceptionOr<FileSystemWritableFileStream::WriteParams> writeParamsFromChunk(FileSystemWritableFileStream::ChunkType&& chunk)
{
    return WTF::switchOn(WTFMove(chunk), [](FileSystemWritableFileStream::WriteParams&& params) -> ExceptionOr<FileSystemWritableFileStream::WriteParams> {
        switch (params.type) {
        case FileSystemWriteCommandType::Write:
            if (!params.data)
                return Exception { ExceptionCode::SyntaxError, "Data is missing"_s };
            return params;
        case FileSystemWriteCommandType::Seek:
            // FIXME: Reconsider exception type when https://github.com/whatwg/fs/issues/168 is closed.
            if (!params.position)
                return Exception { ExceptionCode::SyntaxError, "Position is missing."_s };
            return params;
        case FileSystemWriteCommandType::Truncate:
            // FIXME: Reconsider exception type when https://github.com/whatwg/fs/issues/168 is closed.
            if (!params.size)
                return Exception { ExceptionCode::SyntaxError, "Size is missing."_s };
            return params;
        }
        return params;
    }, [](auto&& data) -> ExceptionOr<FileSystemWritableFileStream::WriteParams> {
        return FileSystemWritableFileStream::WriteParams {
            .type = FileSystemWritableFileStream::WriteCommandType::Write,
            .data = WTFMove(data)
        };
    });
}

static void fetchDataBytesForWrite(const FileSystemWritableFileStream::DataVariant& data, CompletionHandler<void(ExceptionOr<std::span<const uint8_t>>&&)>&& completionHandler)
{
    WTF::switchOn(data, [&](const RefPtr<JSC::ArrayBufferView>& bufferView) {
        if (!bufferView || bufferView->isDetached())
            return completionHandler(Exception { ExceptionCode::TypeError });

        RefPtr buffer = bufferView->possiblySharedBuffer();
        if (!buffer)
            return completionHandler(Exception { ExceptionCode::TypeError });

        completionHandler(buffer->span());
    }, [&](const RefPtr<JSC::ArrayBuffer>& buffer) {
        if (!buffer || buffer->isDetached())
            return completionHandler(Exception { ExceptionCode::TypeError });

        completionHandler(buffer->span());
    }, [&](const RefPtr<Blob>& blob) {
        if (!blob)
            return completionHandler(Exception { ExceptionCode::TypeError });

        // FIXME: For optimization, we may just send blob URL to backend and let it fetch data instead of fetching data here.
        blob->getArrayBuffer([completionHandler = WTFMove(completionHandler)](auto&& result) mutable {
            if (result.hasException())
                return completionHandler(result.releaseException());

            Ref buffer = result.releaseReturnValue();
            if (buffer->isDetached())
                return completionHandler(Exception { ExceptionCode::TypeError });

            completionHandler(buffer->span());
        });
    }, [&](const String& string) {
        completionHandler(byteCast<uint8_t>(string.utf8().span()));
    });
}

ExceptionOr<Ref<FileSystemWritableFileStreamSink>> FileSystemWritableFileStreamSink::create(FileSystemWritableFileStreamIdentifier identifier, FileSystemFileHandle& source)
{
    return adoptRef(*new FileSystemWritableFileStreamSink(identifier, source));
}

FileSystemWritableFileStreamSink::FileSystemWritableFileStreamSink(FileSystemWritableFileStreamIdentifier identifier, FileSystemFileHandle& source)
    : m_identifier(identifier)
    , m_source(source)
{
}

FileSystemWritableFileStreamSink::~FileSystemWritableFileStreamSink()
{
    if (!m_isClosed)
        protectedSource()->closeWritable(m_identifier, FileSystemWriteCloseReason::Completed);
}

static ExceptionOr<FileSystemWritableFileStream::ChunkType> convertFileSystemWritableChunk(ScriptExecutionContext& context, JSC::JSValue value)
{
    auto scope = DECLARE_THROW_SCOPE(context.vm());
    auto chunkResult = convert<IDLUnion<IDLArrayBufferView, IDLArrayBuffer, IDLInterface<Blob>, IDLUSVString, IDLDictionary<FileSystemWritableFileStream::WriteParams>>>(*context.globalObject(), value);
    if (chunkResult.hasException(scope)) [[unlikely]]
        return Exception { ExceptionCode::ExistingExceptionError };

    return chunkResult.releaseReturnValue();
}

// https://fs.spec.whatwg.org/#write-a-chunk
void FileSystemWritableFileStreamSink::write(ScriptExecutionContext& context, JSC::JSValue value, DOMPromiseDeferred<void>&& promise)
{
    ASSERT(!m_isClosed);

    auto chunkResultOrException = convertFileSystemWritableChunk(context, value);
    if (chunkResultOrException.hasException()) {
        promise.reject(chunkResultOrException.releaseException());
        protectedSource()->closeWritable(m_identifier, FileSystemWriteCloseReason::Aborted);
        return;
    }

    auto writeParamsOrException = writeParamsFromChunk(chunkResultOrException.releaseReturnValue());
    if (writeParamsOrException.hasException()) {
        promise.reject(writeParamsOrException.releaseException());
        protectedSource()->closeWritable(m_identifier, FileSystemWriteCloseReason::Aborted);
        return;
    }

    auto writeParams = writeParamsOrException.releaseReturnValue();
    switch (writeParams.type) {
    case FileSystemWriteCommandType::Seek:
    case FileSystemWriteCommandType::Truncate:
        return protectedSource()->executeCommandForWritable(m_identifier, writeParams.type, writeParams.position, writeParams.size, { }, false, WTFMove(promise));
    case FileSystemWriteCommandType::Write:
        if (!writeParams.data)
            return protectedSource()->executeCommandForWritable(m_identifier, writeParams.type, writeParams.position, writeParams.size, { }, true, WTFMove(promise));

        fetchDataBytesForWrite(*writeParams.data, [source = protectedSource(), identifier = m_identifier, promise = WTFMove(promise), type = writeParams.type, size = writeParams.size, position = writeParams.position](auto result) mutable {
            if (result.hasException()) {
                promise.reject(result.releaseException());
                source->closeWritable(identifier, FileSystemWriteCloseReason::Aborted);
                return;
            }
            source->executeCommandForWritable(identifier, type, position, size, result.returnValue(), false, WTFMove(promise));
        });
    }
}

void FileSystemWritableFileStreamSink::close()
{
    ASSERT(!m_isClosed);

    m_isClosed = true;
    protectedSource()->closeWritable(m_identifier, FileSystemWriteCloseReason::Completed);
}

void FileSystemWritableFileStreamSink::abort(JSC::JSValue)
{
    ASSERT(!m_isClosed);

    m_isClosed = true;
    protectedSource()->closeWritable(m_identifier, FileSystemWriteCloseReason::Aborted);
}

} // namespace WebCore
