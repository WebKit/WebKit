/*
 * Copyright (C) 2014-2015 Apple Inc. All rights reserved.
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
#include <dispatch/dispatch.h>
#include <mach/vm_param.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <wtf/BlockPtr.h>
#include <wtf/text/CString.h>

namespace WebKit {
namespace NetworkCache {

IOChannel::IOChannel(const String& filePath, Type type)
    : m_path(filePath)
    , m_type(type)
{
    auto path = FileSystem::fileSystemRepresentation(filePath);
    int oflag;
    mode_t mode;
    bool useLowIOPriority = false;

    switch (m_type) {
    case Type::Create:
        // We don't want to truncate any existing file (with O_TRUNC) as another thread might be mapping it.
        unlink(path.data());
        oflag = O_RDWR | O_CREAT | O_NONBLOCK;
        mode = S_IRUSR | S_IWUSR;
        useLowIOPriority = true;
        break;
    case Type::Write:
        oflag = O_WRONLY | O_NONBLOCK;
        mode = S_IRUSR | S_IWUSR;
        useLowIOPriority = true;
        break;
    case Type::Read:
        oflag = O_RDONLY | O_NONBLOCK;
        mode = 0;
    }

    int fd = ::open(path.data(), oflag, mode);
    m_fileDescriptor = fd;

    m_dispatchIO = adoptOSObject(dispatch_io_create(DISPATCH_IO_RANDOM, fd, dispatch_get_global_queue(useLowIOPriority ? DISPATCH_QUEUE_PRIORITY_BACKGROUND : DISPATCH_QUEUE_PRIORITY_DEFAULT, 0), [fd](int) {
        close(fd);
    }));
    ASSERT(m_dispatchIO.get());

    // This makes the channel read/write all data before invoking the handlers.
    dispatch_io_set_low_water(m_dispatchIO.get(), std::numeric_limits<size_t>::max());
}

IOChannel::~IOChannel()
{
    RELEASE_ASSERT(!m_wasDeleted.exchange(true));
}

Ref<IOChannel> IOChannel::open(const String& filePath, IOChannel::Type type)
{
    return adoptRef(*new IOChannel(filePath, type));
}

void IOChannel::read(size_t offset, size_t size, WorkQueue* queue, Function<void (Data&, int error)>&& completionHandler)
{
    RefPtr<IOChannel> channel(this);
    bool didCallCompletionHandler = false;
    auto dispatchQueue = queue ? queue->dispatchQueue() : dispatch_get_main_queue();
    dispatch_io_read(m_dispatchIO.get(), offset, size, dispatchQueue, makeBlockPtr([channel, completionHandler = WTFMove(completionHandler), didCallCompletionHandler](bool done, dispatch_data_t fileData, int error) mutable {
        ASSERT_UNUSED(done, done || !didCallCompletionHandler);
        if (didCallCompletionHandler)
            return;
        Data data { OSObjectPtr<dispatch_data_t> { fileData } };
        auto callback = WTFMove(completionHandler);
        callback(data, error);
        didCallCompletionHandler = true;
    }).get());
}

void IOChannel::write(size_t offset, const Data& data, WorkQueue* queue, Function<void (int error)>&& completionHandler)
{
    RefPtr<IOChannel> channel(this);
    auto dispatchData = data.dispatchData();
    auto dispatchQueue = queue ? queue->dispatchQueue() : dispatch_get_main_queue();
    dispatch_io_write(m_dispatchIO.get(), offset, dispatchData, dispatchQueue, makeBlockPtr([channel, completionHandler = WTFMove(completionHandler)](bool done, dispatch_data_t fileData, int error) mutable {
        ASSERT_UNUSED(done, done);
        auto callback = WTFMove(completionHandler);
        callback(error);
    }).get());
}

}
}
