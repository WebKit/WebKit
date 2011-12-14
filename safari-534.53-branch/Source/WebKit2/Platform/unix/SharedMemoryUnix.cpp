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

#include "ArgumentDecoder.h"
#include "ArgumentEncoder.h"
#include "WebCoreArgumentCoders.h"
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <wtf/Assertions.h>
#include <wtf/CurrentTime.h>

#if PLATFORM(QT)
#include <QDir>
#elif PLATFORM(GTK)
#include <wtf/gobject/GOwnPtr.h>
#endif

namespace WebKit {

SharedMemory::Handle::Handle()
    : m_fileDescriptor(-1)
    , m_size(0)
{
}

SharedMemory::Handle::~Handle()
{
    if (!isNull())
        while (close(m_fileDescriptor) == -1 && errno == EINTR) { }
}

bool SharedMemory::Handle::isNull() const
{
    return m_fileDescriptor == -1;
}

void SharedMemory::Handle::encode(CoreIPC::ArgumentEncoder* encoder) const
{
    ASSERT(!isNull());

    encoder->encode(releaseToAttachment());
}

bool SharedMemory::Handle::decode(CoreIPC::ArgumentDecoder* decoder, Handle& handle)
{
    ASSERT_ARG(handle, !handle.m_size);
    ASSERT_ARG(handle, handle.isNull());

    CoreIPC::Attachment attachment;
    if (!decoder->decode(attachment))
        return false;

    handle.adoptFromAttachment(attachment.releaseFileDescriptor(), attachment.size());
    return true;
}

CoreIPC::Attachment SharedMemory::Handle::releaseToAttachment() const
{
    ASSERT(!isNull());

    int temp = m_fileDescriptor;
    m_fileDescriptor = -1;
    return CoreIPC::Attachment(temp, m_size);
}

void SharedMemory::Handle::adoptFromAttachment(int fileDescriptor, size_t size)
{
    ASSERT(!m_size);
    ASSERT(isNull());

    m_fileDescriptor = fileDescriptor;
    m_size = size;
}

PassRefPtr<SharedMemory> SharedMemory::create(size_t size)
{
#if PLATFORM(QT)
    QString tempName = QDir::temp().filePath(QLatin1String("qwkshm.XXXXXX"));
    QByteArray tempNameCSTR = tempName.toLocal8Bit();
    char* tempNameC = tempNameCSTR.data();
#elif PLATFORM(GTK)
    GOwnPtr<gchar> tempName(g_build_filename(g_get_tmp_dir(), "WK2SharedMemoryXXXXXX", NULL));
    gchar* tempNameC = tempName.get();
#endif

    int fileDescriptor;
    while ((fileDescriptor = mkstemp(tempNameC)) == -1) {
        if (errno != EINTR)
            return 0;
    }
    while (fcntl(fileDescriptor, F_SETFD, FD_CLOEXEC) == -1) {
        if (errno != EINTR) {
            while (close(fileDescriptor) == -1 && errno == EINTR) { }
            unlink(tempNameC);
            return 0;
        }
    }

    while (ftruncate(fileDescriptor, size) == -1) {
        if (errno != EINTR) {
            while (close(fileDescriptor) == -1 && errno == EINTR) { }
            unlink(tempNameC);
            return 0;
        }
    }

    void* data = mmap(0, size, PROT_READ | PROT_WRITE, MAP_SHARED, fileDescriptor, 0);
    if (data == MAP_FAILED) {
        while (close(fileDescriptor) == -1 && errno == EINTR) { }
        unlink(tempNameC);
        return 0;
    }

    unlink(tempNameC);

    RefPtr<SharedMemory> instance = adoptRef(new SharedMemory());
    instance->m_data = data;
    instance->m_fileDescriptor = fileDescriptor;
    instance->m_size = size;
    return instance.release();
}

static inline int accessModeMMap(SharedMemory::Protection protection)
{
    switch (protection) {
    case SharedMemory::ReadOnly:
        return PROT_READ;
    case SharedMemory::ReadWrite:
        return PROT_READ | PROT_WRITE;
    }

    ASSERT_NOT_REACHED();
    return PROT_READ | PROT_WRITE;
}

PassRefPtr<SharedMemory> SharedMemory::create(const Handle& handle, Protection protection)
{
    ASSERT(!handle.isNull());

    void* data = mmap(0, handle.m_size, accessModeMMap(protection), MAP_SHARED, handle.m_fileDescriptor, 0);
    if (data == MAP_FAILED)
        return 0;

    RefPtr<SharedMemory> instance = adoptRef(new SharedMemory());
    instance->m_data = data;
    instance->m_fileDescriptor = handle.m_fileDescriptor;
    instance->m_size = handle.m_size;
    handle.m_fileDescriptor = -1;
    return instance;
}

SharedMemory::~SharedMemory()
{
    munmap(m_data, m_size);
    while (close(m_fileDescriptor) == -1 && errno == EINTR) { }
}

static inline int accessModeFile(SharedMemory::Protection protection)
{
    switch (protection) {
    case SharedMemory::ReadOnly:
        return O_RDONLY;
    case SharedMemory::ReadWrite:
        return O_RDWR;
    }

    ASSERT_NOT_REACHED();
    return O_RDWR;
}

bool SharedMemory::createHandle(Handle& handle, Protection protection)
{
    ASSERT_ARG(handle, !handle.m_size);
    ASSERT_ARG(handle, handle.isNull());

    int duplicatedHandle;
    while ((duplicatedHandle = dup(m_fileDescriptor)) == -1) {
        if (errno != EINTR) {
            ASSERT_NOT_REACHED();
            return false;
        }
    }

    while ((fcntl(duplicatedHandle, F_SETFD, FD_CLOEXEC | accessModeFile(protection)) == -1)) {
        if (errno != EINTR) {
            ASSERT_NOT_REACHED();
            while (close(duplicatedHandle) == -1 && errno == EINTR) { }
            return false;
        }
    }
    handle.m_fileDescriptor = duplicatedHandle;
    handle.m_size = m_size;
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

