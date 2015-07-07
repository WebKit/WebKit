/*
 * Copyright (C) 2013 Company 100 Inc.
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include "config.h"

#if USE(SOUP)

#include "SharedBuffer.h"

#include "PurgeableBuffer.h"
#include <libsoup/soup.h>

namespace WebCore {

SharedBuffer::SharedBuffer(SoupBuffer* soupBuffer)
    : m_size(0)
    , m_soupBuffer(soupBuffer)
{
    ASSERT(soupBuffer);
}

PassRefPtr<SharedBuffer> SharedBuffer::wrapSoupBuffer(SoupBuffer* soupBuffer)
{
    return adoptRef(new SharedBuffer(soupBuffer));
}

void SharedBuffer::clearPlatformData()
{
    m_soupBuffer.reset();
}

void SharedBuffer::tryReplaceContentsWithPlatformBuffer(SharedBuffer*)
{
    ASSERT_NOT_REACHED();
}

void SharedBuffer::maybeTransferPlatformData()
{
    if (!m_soupBuffer)
        return;

    ASSERT(!m_size);

    // Hang on to the m_soupBuffer pointer in a local pointer as append() will re-enter maybeTransferPlatformData()
    // and we need to make sure to early return when it does.
    GUniquePtr<SoupBuffer> soupBuffer;
    soupBuffer.swap(m_soupBuffer);

    append(soupBuffer->data, soupBuffer->length);
}

bool SharedBuffer::hasPlatformData() const
{
    return m_soupBuffer.get();
}

const char* SharedBuffer::platformData() const
{
    return m_soupBuffer->data;
}

unsigned SharedBuffer::platformDataSize() const
{
    return m_soupBuffer->length;
}

bool SharedBuffer::maybeAppendPlatformData(SharedBuffer*)
{
    return false;
}

} // namespace WebCore

#endif
