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
#include "NetworkCacheData.h"

namespace WebKit {
namespace NetworkCache {

Data::Data(const uint8_t* data, size_t size)
{
    m_buffer.resize(size);
    m_size = size;
    memcpy(m_buffer.data(), data, size);
}

Data::Data(FileSystem::PlatformFileHandle file, size_t offset, size_t size)
{
    m_buffer.resize(size);
    m_size = size;
    FileSystem::seekFile(file, offset, FileSystem::FileSeekOrigin::Beginning);
    FileSystem::readFromFile(file, reinterpret_cast<char*>(m_buffer.data()), size);
    FileSystem::closeFile(file);
}

Data::Data(Vector<uint8_t>&& buffer)
    : m_buffer(WTFMove(buffer))
{
    m_size = m_buffer.size();
}

Data Data::empty()
{
    return { };
}

const uint8_t* Data::data() const
{
    return m_buffer.data();
}

bool Data::isNull() const
{
    return m_buffer.isEmpty();
}

bool Data::apply(const Function<bool(const uint8_t*, size_t)>& applier) const
{
    if (isEmpty())
        return false;

    return applier(reinterpret_cast<const uint8_t*>(m_buffer.data()), m_buffer.size());
}

Data Data::subrange(size_t offset, size_t size) const
{
    return { m_buffer.data() + offset, size };
}

Data concatenate(const Data& a, const Data& b)
{
    Vector<uint8_t> buffer(a.size() + b.size());
    memcpy(buffer.data(), a.data(), a.size());
    memcpy(buffer.data() + a.size(), b.data(), b.size());
    return Data(WTFMove(buffer));
}

} // namespace NetworkCache
} // namespace WebKit
