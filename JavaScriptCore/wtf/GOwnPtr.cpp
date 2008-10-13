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
#include "GOwnPtr.h"

namespace WTF {

template <> void freeOwnedPtr<GError>(GError* ptr)
{
    if (ptr)
        g_error_free(ptr);
}

template <> void freeOwnedPtr<GList>(GList* ptr)
{
    g_list_free(ptr);
}

template <> void freeOwnedPtr<GCond>(GCond* ptr)
{
    if (ptr)
        g_cond_free(ptr);
}

template <> void freeOwnedPtr<GMutex>(GMutex* ptr)
{
    if (ptr)
        g_mutex_free(ptr);
}

template <> void freeOwnedPtr<GPatternSpec>(GPatternSpec* ptr)
{
    if (ptr)
        g_pattern_spec_free(ptr);
}

template <> void freeOwnedPtr<GDir>(GDir* ptr)
{
    if (ptr)
        g_dir_close(ptr);
}

} // namespace WTF
