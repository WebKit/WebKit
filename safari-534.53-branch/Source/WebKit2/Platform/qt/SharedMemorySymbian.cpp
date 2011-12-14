/*
    Copyright (C) 2011 Nokia Corporation and/or its subsidiary(-ies)

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Library General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Library General Public License for more details.

    You should have received a copy of the GNU Library General Public License
    along with this library; see the file COPYING.LIB.  If not, write to
    the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
    Boston, MA 02110-1301, USA.
*/

#include "config.h"
#if PLATFORM(QT) && OS(SYMBIAN)
#include "SharedMemory.h"

#include "ArgumentDecoder.h"
#include "ArgumentEncoder.h"
#include <e32math.h>
#include <qdebug.h>
#include <qglobal.h>
#include <sys/param.h>


namespace WebKit {

SharedMemory::Handle::Handle()
    : m_chunkID(0)
    , m_size(0)
{
}

SharedMemory::Handle::~Handle()
{
}

bool SharedMemory::Handle::isNull() const
{
    return !m_chunkID;
}

void SharedMemory::Handle::encode(CoreIPC::ArgumentEncoder* encoder) const
{
    ASSERT(!isNull());
    encoder->encodeUInt32(m_size);
    // name of the global chunk (masquerading as uint32_t for ease of serialization)
    encoder->encodeUInt32(m_chunkID);
}

bool SharedMemory::Handle::decode(CoreIPC::ArgumentDecoder* decoder, Handle& handle)
{
    size_t size;
    if (!decoder->decodeUInt32(size))
        return false;

    uint32_t chunkID;
    if (!decoder->decodeUInt32(chunkID))
        return false;

    handle.m_size = size;
    handle.m_chunkID = chunkID;
    return true;
}

// FIXME: To be removed as part of Bug 55877
CoreIPC::Attachment SharedMemory::Handle::releaseToAttachment() const
{
    return CoreIPC::Attachment(-1, 0);
}

// FIXME: To be removed as part of Bug 55877
void SharedMemory::Handle::adoptFromAttachment(int, size_t)
{
}

PassRefPtr<SharedMemory> SharedMemory::create(size_t size)
{
    // On Symbian, global chunks (shared memory segments) have system-unique names, so we pick a random
    // number from the kernel's random pool and use it as a string.
    // Using an integer simplifies serialization of the name in Handle::encode()
    uint32_t random = Math::Random();

    TBuf<KMaxKernelName> chunkName;
    chunkName.Format(_L("%d"), random);

    RChunk chunk;
    TInt error = chunk.CreateGlobal(chunkName, size, size);
    if (error) {
        qCritical() << "Failed to create WK2 shared memory of size " << size << " with error " << error;
        return 0;
    }

    RefPtr<SharedMemory> sharedMemory(adoptRef(new SharedMemory));
    sharedMemory->m_handle = chunk.Handle();
    sharedMemory->m_size = chunk.Size();
    sharedMemory->m_data = static_cast<void*>(chunk.Base());
    return sharedMemory.release();
}

PassRefPtr<SharedMemory> SharedMemory::create(const Handle& handle, Protection protection)
{
    if (handle.isNull())
        return 0;

    // Convert number to string, and open the global chunk
    TBuf<KMaxKernelName> chunkName;
    chunkName.Format(_L("%d"), handle.m_chunkID);

    RChunk chunk;
    // NOTE: Symbian OS doesn't support read-only global chunks.
    TInt error = chunk.OpenGlobal(chunkName, false);
    if (error) {
        qCritical() << "Failed to create WK2 shared memory from handle " << error;
        return 0;
    }

    chunk.Adjust(chunk.MaxSize());
    RefPtr<SharedMemory> sharedMemory(adoptRef(new SharedMemory));
    sharedMemory->m_handle = chunk.Handle();
    sharedMemory->m_size = chunk.Size();
    sharedMemory->m_data = static_cast<void*>(chunk.Base());
    return sharedMemory.release();
}

SharedMemory::~SharedMemory()
{
    // FIXME: We don't Close() the chunk here, causing leaks of the shared memory segment
    // If we do, the chunk is closed the decommitted prematurely before the other process
    // has a chance to OpenGlobal() it.
}

bool SharedMemory::createHandle(Handle& handle, Protection protection)
{
    ASSERT_ARG(handle, handle.isNull());

    RChunk chunk;
    if (chunk.SetReturnedHandle(m_handle))
        return false;

    // Convert the name (string form) to a uint32_t.
    TName globalChunkName = chunk.Name();
    TLex lexer(globalChunkName);
    TUint32 nameAsInt = 0;
    if (lexer.Val(nameAsInt, EDecimal))
        return false;

    handle.m_chunkID = nameAsInt;
    handle.m_size = m_size;
    return true;
}

unsigned SharedMemory::systemPageSize()
{
    return PAGE_SIZE;
}

} // namespace WebKit

#endif
