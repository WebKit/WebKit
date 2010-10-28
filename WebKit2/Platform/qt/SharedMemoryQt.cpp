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

#include "SharedMemory.h"

#include "ArgumentDecoder.h"
#include "ArgumentEncoder.h"
#include "MappedMemoryPool.h"
#include "WebCoreArgumentCoders.h"
#include <QIODevice>
#include <unistd.h>
#include <wtf/text/WTFString.h>

namespace WebKit {

static MappedMemoryPool* mappedMemoryPool = MappedMemoryPool::instance();

SharedMemory::Handle::Handle()
    : m_fileName()
    , m_size(0)
{
}

SharedMemory::Handle::~Handle()
{
}

bool SharedMemory::Handle::isNull() const
{
    return m_fileName.isNull();
}

void SharedMemory::Handle::encode(CoreIPC::ArgumentEncoder* encoder) const
{
    encoder->encodeUInt64(m_size);
    encoder->encode(m_fileName);
    m_fileName = String();
}

bool SharedMemory::Handle::decode(CoreIPC::ArgumentDecoder* decoder, Handle& handle)
{
    ASSERT(!handle.m_size);
    ASSERT(handle.m_fileName.isEmpty());

    uint64_t size;
    if (!decoder->decodeUInt64(size))
        return false;

    String fileName;
    if (!decoder->decode(fileName))
       return false;

    handle.m_size = size;
    handle.m_fileName = fileName;

    return true;
}

PassRefPtr<SharedMemory> SharedMemory::create(size_t size)
{
    MappedMemory* mm = mappedMemoryPool->mapMemory(size, QIODevice::ReadWrite);

    RefPtr<SharedMemory> sharedMemory(adoptRef(new SharedMemory));
    sharedMemory->m_size = size;
    sharedMemory->m_data = reinterpret_cast<void*>(mm->data());

    return sharedMemory.release();
}

static inline QIODevice::OpenMode mapProtection(SharedMemory::Protection protection)
{
    switch (protection) {
    case SharedMemory::ReadOnly:
        return QIODevice::ReadOnly;
    case SharedMemory::ReadWrite:
        return QIODevice::ReadWrite;
    }

    ASSERT_NOT_REACHED();
    return QIODevice::NotOpen;
}

PassRefPtr<SharedMemory> SharedMemory::create(const Handle& handle, Protection protection)
{
    if (handle.isNull())
        return 0;

    QIODevice::OpenMode openMode = mapProtection(protection);

    MappedMemory* mm = mappedMemoryPool->mapFile(QString(handle.m_fileName), handle.m_size, openMode);

    RefPtr<SharedMemory> sharedMemory(adoptRef(new SharedMemory));
    sharedMemory->m_size = handle.m_size;
    sharedMemory->m_data = reinterpret_cast<void*>(mm->data());

    return sharedMemory.release();
}

SharedMemory::~SharedMemory()
{
    if (!m_data) {
        // Ownership of the mapped memory has been passed.
        return;
    }

    MappedMemory* mappedMemory = mappedMemoryPool->searchForMappedMemory(reinterpret_cast<uchar*>(m_data));
    if (mappedMemory)
        mappedMemory->markFree();
}

bool SharedMemory::createHandle(Handle& handle, Protection protection)
{
    ASSERT(handle.m_fileName.isNull());
    ASSERT(!handle.m_size);

    MappedMemory* mm = mappedMemoryPool->searchForMappedMemory(reinterpret_cast<uchar*>(m_data));

    if (!mm)
        return false;

    handle.m_fileName = mm->file->fileName();
    handle.m_size = m_size;

    // Hand off ownership of the mapped memory to the receiving process.
    m_data = 0;

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
