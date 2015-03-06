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

#if ENABLE(NETWORK_CACHE)

#include "NetworkCacheFileSystemPosix.h"
#include <dispatch/dispatch.h>
#include <mach/vm_param.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <wtf/text/CString.h>
#include <wtf/text/StringBuilder.h>

namespace WebKit {
namespace NetworkCache {

IOChannel::IOChannel(int fd)
    : m_fileDescriptor(fd)
{
    m_dispatchIO = adoptDispatch(dispatch_io_create(DISPATCH_IO_RANDOM, fd, dispatch_get_main_queue(), [fd](int) {
        close(fd);
    }));
    ASSERT(m_dispatchIO.get());
    // This makes the channel read/write all data before invoking the handlers.
    dispatch_io_set_low_water(m_dispatchIO.get(), std::numeric_limits<size_t>::max());
}

Ref<IOChannel> IOChannel::open(const String& filePath, IOChannel::Type type)
{
    int oflag;
    mode_t mode;

    switch (type) {
    case Type::Create:
        oflag = O_RDWR | O_CREAT | O_TRUNC | O_NONBLOCK;
        mode = S_IRUSR | S_IWUSR;
        break;
    case Type::Write:
        oflag = O_WRONLY | O_NONBLOCK;
        mode = S_IRUSR | S_IWUSR;
        break;
    case Type::Read:
        oflag = O_RDONLY | O_NONBLOCK;
        mode = 0;
    }

    CString path = WebCore::fileSystemRepresentation(filePath);
    int fd = ::open(path.data(), oflag, mode);

    return adoptRef(*new IOChannel(fd));
}

void IOChannel::read(size_t offset, size_t size, std::function<void (Data&, int error)> completionHandler)
{
    RefPtr<IOChannel> channel(this);
    bool didCallCompletionHandler = false;
    dispatch_io_read(m_dispatchIO.get(), offset, size, dispatch_get_main_queue(), [channel, completionHandler, didCallCompletionHandler](bool done, dispatch_data_t fileData, int error) mutable {
        if (done) {
            if (!didCallCompletionHandler) {
                Data nullData;
                completionHandler(nullData, error);
            }
            return;
        }
        ASSERT(!didCallCompletionHandler);
        DispatchPtr<dispatch_data_t> fileDataPtr(fileData);
        Data data(fileDataPtr);
        completionHandler(data, error);
        didCallCompletionHandler = true;
    });
}

// FIXME: It would be better to do without this.
void IOChannel::readSync(size_t offset, size_t size, std::function<void (Data&, int error)> completionHandler)
{
    auto semaphore = adoptDispatch(dispatch_semaphore_create(0));
    read(offset, size, [semaphore, &completionHandler](Data& data, int error) {
        completionHandler(data, error);
        dispatch_semaphore_signal(semaphore.get());
    });
    dispatch_semaphore_wait(semaphore.get(), DISPATCH_TIME_FOREVER);
}

void IOChannel::write(size_t offset, const Data& data, std::function<void (int error)> completionHandler)
{
    RefPtr<IOChannel> channel(this);
    auto dispatchData = data.dispatchData();
    dispatch_io_write(m_dispatchIO.get(), offset, dispatchData, dispatch_get_main_queue(), [channel, completionHandler](bool done, dispatch_data_t fileData, int error) {
        ASSERT_UNUSED(done, done);
        completionHandler(error);
    });
}

}
}

#endif
