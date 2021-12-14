/*
 * Copyright (C) 2008 Collabora Ltd.
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

#include <wtf/glib/GUniquePtr.h>
#include <wtf/text/CString.h>

#include <glib.h>

namespace WebCore {

FragmentedSharedBuffer::FragmentedSharedBuffer(GBytes* bytes)
{
    ASSERT(bytes);
    m_size = g_bytes_get_size(bytes);
    m_segments.append({ 0, DataSegment::create(GRefPtr<GBytes>(bytes)) });
}

Ref<FragmentedSharedBuffer> FragmentedSharedBuffer::create(GBytes* bytes)
{
    return adoptRef(*new FragmentedSharedBuffer(bytes));
}

GRefPtr<GBytes> SharedBuffer::createGBytes() const
{
    ref();
    GRefPtr<GBytes> bytes = adoptGRef(g_bytes_new_with_free_func(data(), size(), [](gpointer data) {
        static_cast<SharedBuffer*>(data)->deref();
    }, const_cast<SharedBuffer*>(this)));
    return bytes;
}

} // namespace WebCore
