/*
 * Copyright (C) 2010 Apple Inc. All rights reserved.
 * Copyright (c) 2010 University of Szeged
 * Copyright (C) 2010 Nokia Corporation and/or its subsidiary(-ies)
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
#if USE(UNIX_DOMAIN_SOCKETS)
#include "SharedMemory.h"

#include "Decoder.h"
#include "Encoder.h"
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <wtf/Assertions.h>
#include <wtf/RandomNumber.h>
#include <wtf/UniStdExtras.h>
#include <wtf/text/CString.h>
#include <wtf/text/WTFString.h>

#if HAVE(LINUX_MEMFD_H)
#include <linux/memfd.h>
#include <sys/syscall.h>
#endif

namespace WebKit {

SharedMemory::Handle::Handle()
{
}

SharedMemory::Handle::~Handle()
{
}

void SharedMemory::Handle::clear()
{
    m_attachment = IPC::Attachment();
}

bool SharedMemory::Handle::isNull() const
{
    return m_attachment.fileDescriptor() == -1;
}

void SharedMemory::Handle::encode(IPC::Encoder& encoder) const
{
    encoder << releaseAttachment();
}

bool SharedMemory::Handle::decode(IPC::Decoder& decoder, Handle& handle)
{
    ASSERT_ARG(handle, handle.isNull());

    IPC::Attachment attachment;
    if (!decoder.decode(attachment))
        return false;

    handle.adoptAttachment(WTFMove(attachment));
    return true;
}

IPC::Attachment SharedMemory::Handle::releaseAttachment() const
{
    return WTFMove(m_attachment);
}

void SharedMemory::Handle::adoptAttachment(IPC::Attachment&& attachment)
{
    ASSERT(isNull());

    m_attachment = WTFMove(attachment);
}

static inline int accessModeMMap(SharedMemory::Protection protection)
{
    switch (protection) {
    case SharedMemory::Protection::ReadOnly:
        return PROT_READ;
    case SharedMemory::Protection::ReadWrite:
        return PROT_READ | PROT_WRITE;
    }

    ASSERT_NOT_REACHED();
    return PROT_READ | PROT_WRITE;
}

static int createSharedMemory()
{
    int fileDescriptor = -1;

#if HAVE(LINUX_MEMFD_H)
    static bool isMemFdAvailable = true;
    if (isMemFdAvailable) {
        do {
            fileDescriptor = syscall(__NR_memfd_create, "WebKitSharedMemory", MFD_CLOEXEC);
        } while (fileDescriptor == -1 && errno == EINTR);

        if (fileDescriptor != -1)
            return fileDescriptor;

        if (errno != ENOSYS)
            return fileDescriptor;

        isMemFdAvailable = false;
    }
#endif

    CString tempName;
    for (int tries = 0; fileDescriptor == -1 && tries < 10; ++tries) {
        String name = String("/WK2SharedMemory.") + String::number(static_cast<unsigned>(WTF::randomNumber() * (std::numeric_limits<unsigned>::max() + 1.0)));
        tempName = name.utf8();

        do {
            fileDescriptor = shm_open(tempName.data(), O_CREAT | O_RDWR, S_IRUSR | S_IWUSR);
        } while (fileDescriptor == -1 && errno == EINTR);
    }

    if (fileDescriptor != -1)
        shm_unlink(tempName.data());

    return fileDescriptor;
}

RefPtr<SharedMemory> SharedMemory::create(void* address, size_t size, Protection protection)
{
    int fileDescriptor = createSharedMemory();
    if (fileDescriptor == -1) {
        WTFLogAlways("Failed to create shared memory: %s", strerror(errno));
        return nullptr;
    }

    while (ftruncate(fileDescriptor, size) == -1) {
        if (errno != EINTR) {
            closeWithRetry(fileDescriptor);
            return nullptr;
        }
    }

    void* data = mmap(address, size, accessModeMMap(protection), MAP_SHARED, fileDescriptor, 0);
    if (data == MAP_FAILED) {
        closeWithRetry(fileDescriptor);
        return nullptr;
    }

    RefPtr<SharedMemory> instance = adoptRef(new SharedMemory());
    instance->m_data = data;
    instance->m_fileDescriptor = fileDescriptor;
    instance->m_size = size;
    return instance;
}

RefPtr<SharedMemory> SharedMemory::allocate(size_t size)
{
    return SharedMemory::create(nullptr, size, SharedMemory::Protection::ReadWrite);
}

RefPtr<SharedMemory> SharedMemory::map(const Handle& handle, Protection protection)
{
    ASSERT(!handle.isNull());

    int fd = handle.m_attachment.releaseFileDescriptor();
    void* data = mmap(0, handle.m_attachment.size(), accessModeMMap(protection), MAP_SHARED, fd, 0);
    closeWithRetry(fd);
    if (data == MAP_FAILED)
        return nullptr;

    RefPtr<SharedMemory> instance = wrapMap(data, handle.m_attachment.size(), -1);
    instance->m_fileDescriptor = std::nullopt;
    instance->m_isWrappingMap = false;
    return instance;
}

RefPtr<SharedMemory> SharedMemory::wrapMap(void* data, size_t size, int fileDescriptor)
{
    RefPtr<SharedMemory> instance = adoptRef(new SharedMemory());
    instance->m_data = data;
    instance->m_size = size;
    instance->m_fileDescriptor = fileDescriptor;
    instance->m_isWrappingMap = true;
    return instance;
}

SharedMemory::~SharedMemory()
{
    if (m_isWrappingMap)
        return;

    munmap(m_data, m_size);
    if (m_fileDescriptor)
        closeWithRetry(m_fileDescriptor.value());
}

bool SharedMemory::createHandle(Handle& handle, Protection)
{
    ASSERT_ARG(handle, handle.isNull());
    ASSERT(m_fileDescriptor);

    // FIXME: Handle the case where the passed Protection is ReadOnly.
    // See https://bugs.webkit.org/show_bug.cgi?id=131542.

    int duplicatedHandle = dupCloseOnExec(m_fileDescriptor.value());
    if (duplicatedHandle == -1) {
        ASSERT_NOT_REACHED();
        return false;
    }
    handle.m_attachment = IPC::Attachment(duplicatedHandle, m_size);
    return true;
}

unsigned SharedMemory::systemPageSize()
{
    static unsigned pageSize = 0;

    if (!pageSize)
        pageSize = getpagesize();

    return pageSize;
}

} // namespace WebKit

#endif

