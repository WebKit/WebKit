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
#include "CleanupHandler.h"
#include "WebCoreArgumentCoders.h"
#include <unistd.h>
#include <QCoreApplication>
#include <QLatin1String>
#include <QSharedMemory>
#include <QString>
#include <QUuid>
#include <wtf/Assertions.h>
#include <wtf/CurrentTime.h>

namespace WebKit {

SharedMemory::Handle::Handle()
    : m_key()
    , m_size(0)
{
}

SharedMemory::Handle::~Handle()
{
}

bool SharedMemory::Handle::isNull() const
{
    return m_key.isNull();
}

void SharedMemory::Handle::encode(CoreIPC::ArgumentEncoder* encoder) const
{
    encoder->encodeUInt64(m_size);
    encoder->encode(m_key);
    m_key = String();
}

bool SharedMemory::Handle::decode(CoreIPC::ArgumentDecoder* decoder, Handle& handle)
{
    ASSERT_ARG(handle, !handle.m_size);
    ASSERT_ARG(handle, handle.m_key.isNull());

    uint64_t size;
    if (!decoder->decodeUInt64(size))
        return false;

    String key;
    if (!decoder->decode(key))
       return false;

    handle.m_size = size;
    handle.m_key = key;

    return true;
}

static QString createUniqueKey()
{
    return QLatin1String("QWKSharedMemoryKey") + QUuid::createUuid().toString();
}

PassRefPtr<SharedMemory> SharedMemory::create(size_t size)
{
    RefPtr<SharedMemory> sharedMemory(adoptRef(new SharedMemory));
    QSharedMemory* impl = new QSharedMemory(createUniqueKey());
    bool created = impl->create(size);
    ASSERT_UNUSED(created, created);

    sharedMemory->m_impl = impl;
    sharedMemory->m_size = size;
    sharedMemory->m_data = impl->data();

    // Do not leave the shared memory segment behind.
    CleanupHandler::instance()->markForCleanup(impl);

    return sharedMemory.release();
}

static inline QSharedMemory::AccessMode accessMode(SharedMemory::Protection protection)
{
    switch (protection) {
    case SharedMemory::ReadOnly:
        return QSharedMemory::ReadOnly;
    case SharedMemory::ReadWrite:
        return QSharedMemory::ReadWrite;
    }

    ASSERT_NOT_REACHED();
    return QSharedMemory::ReadWrite;
}

PassRefPtr<SharedMemory> SharedMemory::create(const Handle& handle, Protection protection)
{
    if (handle.isNull())
        return 0;

    QSharedMemory* impl = new QSharedMemory(QString(handle.m_key));
    bool attached = impl->attach(accessMode(protection));
    if (!attached) {
        delete impl;
        return 0;
    }

    RefPtr<SharedMemory> sharedMemory(adoptRef(new SharedMemory));
    sharedMemory->m_impl = impl;
    ASSERT(handle.m_size == impl->size());
    sharedMemory->m_size = handle.m_size;
    sharedMemory->m_data = impl->data();

    // Do not leave the shared memory segment behind.
    CleanupHandler::instance()->markForCleanup(impl);

    return sharedMemory.release();
}

SharedMemory::~SharedMemory()
{
    if (CleanupHandler::instance()->hasStartedDeleting())
        return;

    CleanupHandler::instance()->unmark(m_impl);
    delete m_impl;
}

bool SharedMemory::createHandle(Handle& handle, Protection protection)
{
    ASSERT_ARG(handle, handle.m_key.isNull());
    ASSERT_ARG(handle, !handle.m_size);

    QString key = m_impl->key();
    if (key.isNull())
        return false;
    handle.m_key = String(key);
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
