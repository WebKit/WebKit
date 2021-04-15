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

#include <libsoup/soup.h>

namespace WebCore {

SharedBuffer::SharedBuffer(SoupBuffer* soupBuffer)
{
    ASSERT(soupBuffer);
    m_size = soupBuffer->length;
    m_segments.append({0, DataSegment::create(GUniquePtr<SoupBuffer>(soupBuffer))});
}

Ref<SharedBuffer> SharedBuffer::wrapSoupBuffer(SoupBuffer* soupBuffer)
{
    return adoptRef(*new SharedBuffer(soupBuffer));
}

GUniquePtr<SoupBuffer> SharedBuffer::createSoupBuffer(unsigned offset, unsigned size)
{
    ref();
    GUniquePtr<SoupBuffer> buffer(soup_buffer_new_with_owner(data() + offset, size ? size : this->size(), this, [](void* data) {
        static_cast<SharedBuffer*>(data)->deref();
    }));
    return buffer;
}

} // namespace WebCore

#endif
