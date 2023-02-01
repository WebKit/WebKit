/*
 * Copyright (C) 2018 Sony Interactive Entertainment Inc.
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

#include <wtf/RunLoop.h>

namespace WebKit {
namespace NetworkCache {

IOChannel::IOChannel(String&& filePath, Type type, std::optional<WorkQueue::QOS>)
    : m_path(WTFMove(filePath))
    , m_type(type)
{
    FileSystem::FileOpenMode mode { };
    switch (type) {
    case Type::Read:
        mode = FileSystem::FileOpenMode::Read;
        break;
    case Type::Write:
        mode = FileSystem::FileOpenMode::ReadWrite;
        break;
    case Type::Create:
        mode = FileSystem::FileOpenMode::ReadWrite;
        break;
    }
    m_fileDescriptor = FileSystem::openFile(m_path, mode);
}

IOChannel::~IOChannel()
{
    FileSystem::closeFile(m_fileDescriptor);
}

void IOChannel::read(size_t offset, size_t size, WTF::WorkQueueBase& queue, Function<void(Data&, int error)>&& completionHandler)
{
    queue.dispatch([this, protectedThis = Ref { *this }, offset, size, completionHandler = WTFMove(completionHandler)] {
        auto fileSize = FileSystem::fileSize(m_fileDescriptor);
        if (!fileSize || *fileSize > std::numeric_limits<size_t>::max()) {
            Data data;
            completionHandler(data, -1);
            return;
        }
        size_t readSize = *fileSize;
        readSize = std::min(size, readSize);
        Vector<uint8_t> buffer(readSize);
        FileSystem::seekFile(m_fileDescriptor, offset, FileSystem::FileSeekOrigin::Beginning);
        int err = FileSystem::readFromFile(m_fileDescriptor, buffer.data(), readSize);
        err = err < 0 ? err : 0;
        auto data = Data(WTFMove(buffer));
        completionHandler(data, err);
    });
}

void IOChannel::write(size_t offset, const Data& data, WTF::WorkQueueBase& queue, Function<void(int error)>&& completionHandler)
{
    queue.dispatch([this, protectedThis = Ref { *this }, offset, data, completionHandler = WTFMove(completionHandler)] {
        FileSystem::seekFile(m_fileDescriptor, offset, FileSystem::FileSeekOrigin::Beginning);
        int err = FileSystem::writeToFile(m_fileDescriptor, data.data(), data.size());
        err = err < 0 ? err : 0;
        completionHandler(err);
    });
}

} // namespace NetworkCache
} // namespace WebKit
