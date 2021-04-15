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

IOChannel::IOChannel(const String& filePath, Type type, Optional<WorkQueue::QOS>)
    : m_path(filePath)
    , m_type(type)
{
    auto path = FileSystem::fileSystemRepresentation(filePath);
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

static inline void runTaskInQueue(Function<void ()>&& task, WorkQueue* queue)
{
    if (queue) {
        queue->dispatch(WTFMove(task));
        return;
    }

    // Using nullptr as queue submits the result to the main context.
    RunLoop::main().dispatch(WTFMove(task));
}

static void fillDataFromReadBuffer(SoupBuffer* readBuffer, size_t size, Data& data)
{
    GRefPtr<SoupBuffer> buffer;
    if (size != readBuffer->length) {
        // The subbuffer does not copy the data.
        buffer = adoptGRef(soup_buffer_new_subbuffer(readBuffer, 0, size));
    } else
        buffer = readBuffer;

    if (data.isNull()) {
        // First chunk, we need to force the data to be copied.
        data = { reinterpret_cast<const uint8_t*>(buffer->data), size };
    } else {
        Data dataRead(WTFMove(buffer));
        // Concatenate will copy the data.
        data = concatenate(data, dataRead);
    }
}

struct ReadAsyncData {
    RefPtr<IOChannel> channel;
    GRefPtr<SoupBuffer> buffer;
    RefPtr<WorkQueue> queue;
    size_t bytesToRead;
    Function<void (Data&, int error)> completionHandler;
    Data data;
};

static void inputStreamReadReadyCallback(GInputStream* stream, GAsyncResult* result, gpointer userData)
{
    std::unique_ptr<ReadAsyncData> asyncData(static_cast<ReadAsyncData*>(userData));
    gssize bytesRead = g_input_stream_read_finish(stream, result, nullptr);
    if (bytesRead == -1) {
        WorkQueue* queue = asyncData->queue.get();
        runTaskInQueue([asyncData = WTFMove(asyncData)] {
            asyncData->completionHandler(asyncData->data, -1);
        }, queue);
        return;
    }

    if (!bytesRead) {
        WorkQueue* queue = asyncData->queue.get();
        runTaskInQueue([asyncData = WTFMove(asyncData)] {
            asyncData->completionHandler(asyncData->data, 0);
        }, queue);
        return;
    }

    ASSERT(bytesRead > 0);
    fillDataFromReadBuffer(asyncData->buffer.get(), static_cast<size_t>(bytesRead), asyncData->data);

    size_t pendingBytesToRead = asyncData->bytesToRead - asyncData->data.size();
    if (!pendingBytesToRead) {
        WorkQueue* queue = asyncData->queue.get();
        runTaskInQueue([asyncData = WTFMove(asyncData)] {
            asyncData->completionHandler(asyncData->data, 0);
        }, queue);
        return;
    }

    size_t bytesToRead = std::min(pendingBytesToRead, asyncData->buffer->length);
    // Use a local variable for the data buffer to pass it to g_input_stream_read_async(), because ReadAsyncData is released.
    auto data = const_cast<char*>(asyncData->buffer->data);
    g_input_stream_read_async(stream, data, bytesToRead, RunLoopSourcePriority::DiskCacheRead, nullptr,
        reinterpret_cast<GAsyncReadyCallback>(inputStreamReadReadyCallback), asyncData.release());
}

void IOChannel::read(size_t offset, size_t size, WorkQueue* queue, Function<void (Data&, int error)>&& completionHandler)
{
    RefPtr<IOChannel> channel(this);
    if (!m_inputStream) {
        runTaskInQueue([channel, completionHandler = WTFMove(completionHandler)] {
            Data data;
            completionHandler(data, -1);
        }, queue);
        return;
    }

    if (!RunLoop::isMain()) {
        readSyncInThread(offset, size, queue, WTFMove(completionHandler));
        return;
    }

    size_t bufferSize = std::min(size, gDefaultReadBufferSize);
    uint8_t* bufferData = static_cast<uint8_t*>(fastMalloc(bufferSize));
    GRefPtr<SoupBuffer> buffer = adoptGRef(soup_buffer_new_with_owner(bufferData, bufferSize, bufferData, fastFree));
    ReadAsyncData* asyncData = new ReadAsyncData { this, buffer.get(), queue, size, WTFMove(completionHandler), { } };

    // FIXME: implement offset.
    g_input_stream_read_async(m_inputStream.get(), const_cast<char*>(buffer->data), bufferSize, RunLoopSourcePriority::DiskCacheRead, nullptr,
        reinterpret_cast<GAsyncReadyCallback>(inputStreamReadReadyCallback), asyncData);
}

void IOChannel::readSyncInThread(size_t offset, size_t size, WorkQueue* queue, Function<void (Data&, int error)>&& completionHandler)
{
    ASSERT(!RunLoop::isMain());

    RefPtr<IOChannel> channel(this);
    Thread::create("IOChannel::readSync", [channel, size, queue, completionHandler = WTFMove(completionHandler)] () mutable {
        size_t bufferSize = std::min(size, gDefaultReadBufferSize);
        uint8_t* bufferData = static_cast<uint8_t*>(fastMalloc(bufferSize));
        GRefPtr<SoupBuffer> readBuffer = adoptGRef(soup_buffer_new_with_owner(bufferData, bufferSize, bufferData, fastFree));
        Data data;
        size_t pendingBytesToRead = size;
        size_t bytesToRead = bufferSize;
        do {
            // FIXME: implement offset.
            gssize bytesRead = g_input_stream_read(channel->m_inputStream.get(), const_cast<char*>(readBuffer->data), bytesToRead, nullptr, nullptr);
            if (bytesRead == -1) {
                runTaskInQueue([channel, completionHandler = WTFMove(completionHandler)] {
                    Data data;
                    completionHandler(data, -1);
                }, queue);
                return;
            }

            if (!bytesRead)
                break;

            ASSERT(bytesRead > 0);
            fillDataFromReadBuffer(readBuffer.get(), static_cast<size_t>(bytesRead), data);

            pendingBytesToRead = size - data.size();
            bytesToRead = std::min(pendingBytesToRead, readBuffer->length);
        } while (pendingBytesToRead);

        GRefPtr<SoupBuffer> bufferCapture = data.soupBuffer();
        runTaskInQueue([channel, bufferCapture, completionHandler = WTFMove(completionHandler)] {
            GRefPtr<SoupBuffer> buffer = bufferCapture;
            Data data = { WTFMove(buffer) };
            completionHandler(data, 0);
        }, queue);
    })->detach();
}

struct WriteAsyncData {
    RefPtr<IOChannel> channel;
    GRefPtr<SoupBuffer> buffer;
    RefPtr<WorkQueue> queue;
    Function<void (int error)> completionHandler;
};

static void outputStreamWriteReadyCallback(GOutputStream* stream, GAsyncResult* result, gpointer userData)
{
    std::unique_ptr<WriteAsyncData> asyncData(static_cast<WriteAsyncData*>(userData));
    gssize bytesWritten = g_output_stream_write_finish(stream, result, nullptr);
    if (bytesWritten == -1) {
        WorkQueue* queue = asyncData->queue.get();
        runTaskInQueue([asyncData = WTFMove(asyncData)] {
            asyncData->completionHandler(-1);
        }, queue);
        return;
    }

    gssize pendingBytesToWrite = asyncData->buffer->length - bytesWritten;
    if (!pendingBytesToWrite) {
        WorkQueue* queue = asyncData->queue.get();
        runTaskInQueue([asyncData = WTFMove(asyncData)] {
            asyncData->completionHandler(0);
        }, queue);
        return;
    }

    asyncData->buffer = adoptGRef(soup_buffer_new_subbuffer(asyncData->buffer.get(), bytesWritten, pendingBytesToWrite));
    // Use a local variable for the data buffer to pass it to g_output_stream_write_async(), because WriteAsyncData is released.
    auto data = asyncData->buffer->data;
    g_output_stream_write_async(stream, data, pendingBytesToWrite, RunLoopSourcePriority::DiskCacheWrite, nullptr,
        reinterpret_cast<GAsyncReadyCallback>(outputStreamWriteReadyCallback), asyncData.release());
}

void IOChannel::write(size_t offset, const Data& data, WorkQueue* queue, Function<void (int error)>&& completionHandler)
{
    RefPtr<IOChannel> channel(this);
    if (!m_outputStream && !m_ioStream) {
        runTaskInQueue([channel, completionHandler = WTFMove(completionHandler)] {
            completionHandler(-1);
        }, queue);
        return;
    }

    GOutputStream* stream = m_outputStream ? m_outputStream.get() : g_io_stream_get_output_stream(G_IO_STREAM(m_ioStream.get()));
    if (!stream) {
        runTaskInQueue([channel, completionHandler = WTFMove(completionHandler)] {
            completionHandler(-1);
        }, queue);
        return;
    }

    WriteAsyncData* asyncData = new WriteAsyncData { this, data.soupBuffer(), queue, WTFMove(completionHandler) };
    // FIXME: implement offset.
    g_output_stream_write_async(stream, asyncData->buffer->data, data.size(), RunLoopSourcePriority::DiskCacheWrite, nullptr,
        reinterpret_cast<GAsyncReadyCallback>(outputStreamWriteReadyCallback), asyncData);
}

} // namespace NetworkCache
} // namespace WebKit
