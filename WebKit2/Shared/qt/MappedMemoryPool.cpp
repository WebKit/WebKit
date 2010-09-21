/*
 * Copyright (C) 2010 University of Szeged
 * All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY UNIVERSITY OF SZEGED ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL UNIVERSITY OF SZEGED OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "MappedMemory.h"

#include "StdLibExtras.h"

namespace WebKit {

MappedMemoryPool::MappedMemoryPool()
{
    connect(QCoreApplication::instance(), SIGNAL(aboutToQuit()), SLOT(cleanUp()));
}

MappedMemoryPool* MappedMemoryPool::instance()
{
    DEFINE_STATIC_LOCAL(MappedMemoryPool, singleton, ());
    return &singleton;
}

size_t MappedMemoryPool::size() const
{
    return m_pool.size();
}

MappedMemory& MappedMemoryPool::at(size_t i)
{
    return m_pool.at(i);
}

MappedMemory& MappedMemoryPool::append(const MappedMemory& newMap)
{
    m_pool.append(newMap);
    return m_pool.last();
}

void MappedMemoryPool::cleanUp()
{
    size_t size = m_pool.size();

    for (size_t i = 0; i < size; ++i) {
        MappedMemory& chunk = m_pool.at(i);
        if (!chunk.isFree())
            chunk.file->unmap(chunk.data);
        chunk.file->remove();
    }

    delete this;
}

} // namespace WebKit
