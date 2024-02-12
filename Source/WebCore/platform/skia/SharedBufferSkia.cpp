/*
 * Copyright (C) 2024 Igalia S.L.
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
#include "SharedBuffer.h"

namespace WebCore {

FragmentedSharedBuffer::FragmentedSharedBuffer(SkData* data)
{
    ASSERT(data);
    m_size = data->size();
    m_segments.append({ 0, DataSegment::create(sk_ref_sp(data)) });
}

Ref<FragmentedSharedBuffer> FragmentedSharedBuffer::create(SkData* data)
{
    return adoptRef(*new FragmentedSharedBuffer(data));
}

sk_sp<SkData> SharedBuffer::createSkData() const
{
    ref();
    return SkData::MakeWithProc(data(), size(), [](const void*, void* context) {
        static_cast<SharedBuffer*>(context)->deref();
    }, const_cast<SharedBuffer*>(this));
}

} // namespace WebCore
