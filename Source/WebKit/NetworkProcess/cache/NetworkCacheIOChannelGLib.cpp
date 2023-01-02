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

IOChannel::IOChannel(String&& filePath, Type type, std::optional<WorkQueue::QOS> qos)
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
        m_qos = qos.value_or(WorkQueue::QOS::Background);
        break;
    }
    case Type::Write: {
        auto ioStream = adoptGRef(g_file_open_readwrite(file.get(), nullptr, nullptr));
        m_outputStream = g_io_stream_get_output_stream(G_IO_STREAM(ioStream.get()));
        m_qos = qos.value_or(WorkQueue::QOS::Background);
        break;
    }
    case Type::Read:
        m_inputStream = adoptGRef(G_INPUT_STREAM(g_file_read(file.get(), nullptr, nullptr)));
        m_qos = qos.value_or(WorkQueue::QOS::Default);
        break;
    }
}

IOChannel::~IOChannel()
{
    RELEASE_ASSERT(!m_wasDeleted.exchange(true));
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

    Thread::create("IOChannel::read", [this, protectedThis = WTFMove(protectedThis), offset, size, queue = Ref { queue }, completionHandler = WTFMove(completionHandler)]() mutable {
        GRefPtr<GFileInfo> info = adoptGRef(g_file_input_stream_query_info(G_FILE_INPUT_STREAM(m_inputStream.get()), G_FILE_ATTRIBUTE_STANDARD_SIZE, nullptr, nullptr));
        if (info) {
            auto fileSize = g_file_info_get_size(info.get());
            if (fileSize && static_cast<guint64>(fileSize) <= std::numeric_limits<size_t>::max()) {
                if (G_IS_SEEKABLE(m_inputStream.get()) && g_seekable_can_seek(G_SEEKABLE(m_inputStream.get())))
                    g_seekable_seek(G_SEEKABLE(m_inputStream.get()), offset, G_SEEK_SET, nullptr, nullptr);

                size_t bufferSize = std::min<size_t>(size, fileSize - offset);
                uint8_t* bufferData = static_cast<uint8_t*>(fastMalloc(bufferSize));
                GRefPtr<GBytes> buffer = adoptGRef(g_bytes_new_with_free_func(bufferData, bufferSize, fastFree, bufferData));
                gsize bytesRead;
                if (g_input_stream_read_all(m_inputStream.get(), bufferData, bufferSize, &bytesRead, nullptr, nullptr)) {
                    GRefPtr<GBytes> bytes = bufferSize == bytesRead ? buffer : adoptGRef(g_bytes_new_from_bytes(buffer.get(), 0, bytesRead));
                    queue->dispatch([protectedThis = WTFMove(protectedThis), bytes = WTFMove(bytes), completionHandler = WTFMove(completionHandler)]() mutable {
                        Data data(WTFMove(bytes));
                        completionHandler(data, 0);
                    });
                    return;
                }
            }
        }
        queue->dispatch([protectedThis = WTFMove(protectedThis), completionHandler = WTFMove(completionHandler)] {
            Data data;
            completionHandler(data, -1);
        });
    }, ThreadType::Unknown, m_qos)->detach();
}

void IOChannel::write(size_t offset, const Data& data, WTF::WorkQueueBase& queue, Function<void(int error)>&& completionHandler)
{
    RefPtr<IOChannel> protectedThis(this);
    if (!m_outputStream) {
        queue.dispatch([protectedThis = WTFMove(protectedThis), completionHandler = WTFMove(completionHandler)] {
            completionHandler(-1);
        });
        return;
    }

    GRefPtr<GBytes> buffer = offset ? adoptGRef(g_bytes_new_from_bytes(data.bytes(), offset, data.size() - offset)) : data.bytes();
    Thread::create("IOChannel::write", [this, protectedThis = WTFMove(protectedThis), buffer = WTFMove(buffer), queue = Ref { queue }, completionHandler = WTFMove(completionHandler)]() mutable {
        gsize buffersize;
        const auto* bufferData = g_bytes_get_data(buffer.get(), &buffersize);
        auto success = g_output_stream_write_all(m_outputStream.get(), bufferData, buffersize, nullptr, nullptr, nullptr);
        queue->dispatch([protectedThis = WTFMove(protectedThis), success, completionHandler = WTFMove(completionHandler)] {
            completionHandler(success ? 0 : -1);
        });
    }, ThreadType::Unknown, m_qos)->detach();
}

} // namespace NetworkCache
} // namespace WebKit
