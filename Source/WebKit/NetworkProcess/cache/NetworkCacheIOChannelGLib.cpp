/*
 * Copyright (C) 2015 Igalia S.L.
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
#include "NetworkCacheIOChannel.h"

#include "NetworkCacheFileSystem.h"
#include <wtf/MainThread.h>
#include <wtf/RunLoop.h>
#include <wtf/glib/GUniquePtr.h>
#include <wtf/glib/RunLoopSourcePriority.h>

namespace WebKit {
namespace NetworkCache {

static const size_t gDefaultReadBufferSize = 4096;

IOChannel::IOChannel(String&& filePath, Type type, std::optional<WorkQueue::QOS>)
    : m_path(WTFMove(filePath))
    , m_type(type)
{
    auto path = FileSystem::fileSystemRepresentation(m_path);
    GRefPtr<GFile> file = adoptGRef(g_file_new_for_path(path.data()));
    switch (m_type) {
    case Type::Create: {
        g_file_delete(file.get(), nullptr, nullptr);
        m_outputStream = adoptGRef(G_OUTPUT_STREAM(g_file_create(file.get(), static_cast<GFileCreateFlags>(G_FILE_CREATE_PRIVATE), nullptr, nullptr)));
#if !HAVE(STAT_BIRTHTIME)
        GUniquePtr<char> birthtimeString(g_strdup_printf("%" G_GUINT64_FORMAT, WallTime::now().secondsSinceEpoch().secondsAs<uint64_t>()));
        g_file_set_attribute_string(file.get(), "xattr::birthtime", birthtimeString.get(), G_FILE_QUERY_INFO_NONE, nullptr, nullptr);
#endif
        break;
    }
    case Type::Write: {
        m_ioStream = adoptGRef(g_file_open_readwrite(file.get(), nullptr, nullptr));
        break;
    }
    case Type::Read:
        m_inputStream = adoptGRef(G_INPUT_STREAM(g_file_read(file.get(), nullptr, nullptr)));
        break;
    }
}

IOChannel::~IOChannel()
{
    RELEASE_ASSERT(!m_wasDeleted.exchange(true));
}

static void fillDataFromReadBuffer(GBytes* readBuffer, size_t size, Data& data)
{
    GRefPtr<GBytes> buffer;
    if (size != g_bytes_get_size(readBuffer)) {
        // The subbuffer does not copy the data.
        buffer = adoptGRef(g_bytes_new_from_bytes(readBuffer, 0, size));
    } else
        buffer = readBuffer;

    if (data.isNull()) {
        // First chunk, we need to force the data to be copied.
        data = { reinterpret_cast<const uint8_t*>(g_bytes_get_data(buffer.get(), nullptr)), size };
    } else {
        Data dataRead(WTFMove(buffer));
        // Concatenate will copy the data.
        data = concatenate(data, dataRead);
    }
}

struct ReadAsyncData {
    WTF_MAKE_STRUCT_FAST_ALLOCATED;

    RefPtr<IOChannel> channel;
    GRefPtr<GBytes> buffer;
    Ref<WTF::WorkQueueBase> queue;
    size_t bytesToRead;
    Function<void(Data&, int error)> completionHandler;
    Data data;
};

static void inputStreamReadReadyCallback(GInputStream* stream, GAsyncResult* result, gpointer userData)
{
    std::unique_ptr<ReadAsyncData> asyncData(static_cast<ReadAsyncData*>(userData));
    gssize bytesRead = g_input_stream_read_finish(stream, result, nullptr);
    if (bytesRead == -1) {
        asyncData->queue->dispatch([asyncData = WTFMove(asyncData)] {
            asyncData->completionHandler(asyncData->data, -1);
        });
        return;
    }

    if (!bytesRead) {
        asyncData->queue->dispatch([asyncData = WTFMove(asyncData)] {
            asyncData->completionHandler(asyncData->data, 0);
        });
        return;
    }

    ASSERT(bytesRead > 0);
    fillDataFromReadBuffer(asyncData->buffer.get(), static_cast<size_t>(bytesRead), asyncData->data);

    size_t pendingBytesToRead = asyncData->bytesToRead - asyncData->data.size();
    if (!pendingBytesToRead) {
        asyncData->queue->dispatch([asyncData = WTFMove(asyncData)] {
            asyncData->completionHandler(asyncData->data, 0);
        });
        return;
    }

    size_t bytesToRead = std::min(pendingBytesToRead, g_bytes_get_size(asyncData->buffer.get()));
    // Use a local variable for the data buffer to pass it to g_input_stream_read_async(), because ReadAsyncData is released.
    auto* data = const_cast<void*>(g_bytes_get_data(asyncData->buffer.get(), nullptr));
    g_input_stream_read_async(stream, data, bytesToRead, RunLoopSourcePriority::DiskCacheRead, nullptr,
        reinterpret_cast<GAsyncReadyCallback>(inputStreamReadReadyCallback), asyncData.release());
}

void IOChannel::read(size_t offset, size_t size, WTF::WorkQueueBase& queue, Function<void(Data&, int error)>&& completionHandler)
{
    RefPtr<IOChannel> protectedThis(this);
    if (!m_inputStream) {
        queue.dispatch([protectedThis = WTFMove(protectedThis), completionHandler = WTFMove(completionHandler)] {
            Data data;
            completionHandler(data, -1);
        });
        return;
    }

    if (!RunLoop::isMain()) {
        readSyncInThread(offset, size, queue, WTFMove(completionHandler));
        return;
    }

    size_t bufferSize = std::min(size, gDefaultReadBufferSize);
    uint8_t* bufferData = static_cast<uint8_t*>(fastMalloc(bufferSize));
    GRefPtr<GBytes> buffer = adoptGRef(g_bytes_new_with_free_func(bufferData, bufferSize, fastFree, bufferData));
    ReadAsyncData* asyncData = new ReadAsyncData { this, buffer.get(), queue, size, WTFMove(completionHandler), { } };

    // FIXME: implement offset.
    g_input_stream_read_async(m_inputStream.get(), bufferData, bufferSize, RunLoopSourcePriority::DiskCacheRead, nullptr,
        reinterpret_cast<GAsyncReadyCallback>(inputStreamReadReadyCallback), asyncData);
}

void IOChannel::readSyncInThread(size_t offset, size_t size, WTF::WorkQueueBase& queue, Function<void(Data&, int error)>&& completionHandler)
{
    ASSERT(!RunLoop::isMain());

    Thread::create("IOChannel::readSync", [this, protectedThis = Ref { *this }, size, queue = Ref { queue }, completionHandler = WTFMove(completionHandler)] () mutable {
        size_t bufferSize = std::min(size, gDefaultReadBufferSize);
        uint8_t* bufferData = static_cast<uint8_t*>(fastMalloc(bufferSize));
        GRefPtr<GBytes> readBuffer = adoptGRef(g_bytes_new_with_free_func(bufferData, bufferSize, fastFree, bufferData));
        Data data;
        size_t pendingBytesToRead = size;
        size_t bytesToRead = bufferSize;
        do {
            // FIXME: implement offset.
            gssize bytesRead = g_input_stream_read(m_inputStream.get(), const_cast<void*>(g_bytes_get_data(readBuffer.get(), nullptr)), bytesToRead, nullptr, nullptr);
            if (bytesRead == -1) {
                queue->dispatch([protectedThis = WTFMove(protectedThis), completionHandler = WTFMove(completionHandler)] {
                    Data data;
                    completionHandler(data, -1);
                });
                return;
            }

            if (!bytesRead)
                break;

            ASSERT(bytesRead > 0);
            fillDataFromReadBuffer(readBuffer.get(), static_cast<size_t>(bytesRead), data);

            pendingBytesToRead = size - data.size();
            bytesToRead = std::min(pendingBytesToRead, g_bytes_get_size(readBuffer.get()));
        } while (pendingBytesToRead);

        queue->dispatch([protectedThis = WTFMove(protectedThis), buffer = GRefPtr<GBytes>(data.bytes()), completionHandler = WTFMove(completionHandler)]() mutable {
            Data data = { WTFMove(buffer) };
            completionHandler(data, 0);
        });
    })->detach();
}

struct WriteAsyncData {
    WTF_MAKE_STRUCT_FAST_ALLOCATED;

    RefPtr<IOChannel> channel;
    GRefPtr<GBytes> buffer;
    Ref<WTF::WorkQueueBase> queue;
    Function<void(int error)> completionHandler;
};

static void outputStreamWriteReadyCallback(GOutputStream* stream, GAsyncResult* result, gpointer userData)
{
    std::unique_ptr<WriteAsyncData> asyncData(static_cast<WriteAsyncData*>(userData));
    gssize bytesWritten = g_output_stream_write_finish(stream, result, nullptr);
    if (bytesWritten == -1) {
        asyncData->queue->dispatch([asyncData = WTFMove(asyncData)] {
            asyncData->completionHandler(-1);
        });
        return;
    }

    gssize pendingBytesToWrite = g_bytes_get_size(asyncData->buffer.get()) - bytesWritten;
    if (!pendingBytesToWrite) {
        asyncData->queue->dispatch([asyncData = WTFMove(asyncData)] {
            asyncData->completionHandler(0);
        });
        return;
    }

    asyncData->buffer = adoptGRef(g_bytes_new_from_bytes(asyncData->buffer.get(), bytesWritten, pendingBytesToWrite));
    // Use a local variable for the data buffer to pass it to g_output_stream_write_async(), because WriteAsyncData is released.
    const auto* data = g_bytes_get_data(asyncData->buffer.get(), nullptr);
    g_output_stream_write_async(stream, data, pendingBytesToWrite, RunLoopSourcePriority::DiskCacheWrite, nullptr,
        reinterpret_cast<GAsyncReadyCallback>(outputStreamWriteReadyCallback), asyncData.release());
}

void IOChannel::write(size_t offset, const Data& data, WTF::WorkQueueBase& queue, Function<void(int error)>&& completionHandler)
{
    RefPtr<IOChannel> protectedThis(this);
    if (!m_outputStream && !m_ioStream) {
        queue.dispatch([protectedThis = WTFMove(protectedThis), completionHandler = WTFMove(completionHandler)] {
            completionHandler(-1);
        });
        return;
    }

    GOutputStream* stream = m_outputStream ? m_outputStream.get() : g_io_stream_get_output_stream(G_IO_STREAM(m_ioStream.get()));
    if (!stream) {
        queue.dispatch([protectedThis = WTFMove(protectedThis), completionHandler = WTFMove(completionHandler)] {
            completionHandler(-1);
        });
        return;
    }

    WriteAsyncData* asyncData = new WriteAsyncData { this, data.bytes(), queue, WTFMove(completionHandler) };
    // FIXME: implement offset.
    g_output_stream_write_async(stream, g_bytes_get_data(asyncData->buffer.get(), nullptr), data.size(), RunLoopSourcePriority::DiskCacheWrite, nullptr,
        reinterpret_cast<GAsyncReadyCallback>(outputStreamWriteReadyCallback), asyncData);
}

} // namespace NetworkCache
} // namespace WebKit
