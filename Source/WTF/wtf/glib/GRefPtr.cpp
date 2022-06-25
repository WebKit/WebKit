/*
 * Copyright (C) 2009 Martin Robinson
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
#include <wtf/glib/GRefPtr.h>

#if USE(GLIB)

#include <glib-object.h>
#include <glib.h>

namespace WTF {

template <> GHashTable* refGPtr(GHashTable* ptr)
{
    if (ptr)
        g_hash_table_ref(ptr);
    return ptr;
}

template <> void derefGPtr(GHashTable* ptr)
{
    if (ptr)
        g_hash_table_unref(ptr);
}

template <> GMainContext* refGPtr(GMainContext* ptr)
{
    if (ptr)
        g_main_context_ref(ptr);
    return ptr;
}

template <> void derefGPtr(GMainContext* ptr)
{
    if (ptr)
        g_main_context_unref(ptr);
}

template <> GMainLoop* refGPtr(GMainLoop* ptr)
{
    if (ptr)
        g_main_loop_ref(ptr);
    return ptr;
}

template <> void derefGPtr(GMainLoop* ptr)
{
    if (ptr)
        g_main_loop_unref(ptr);
}

template <> GBytes* refGPtr(GBytes* ptr)
{
    if (ptr)
        g_bytes_ref(ptr);
    return ptr;
}

template <> void derefGPtr(GBytes* ptr)
{
    if (ptr)
        g_bytes_unref(ptr);
}

template <> GVariant* refGPtr(GVariant* ptr)
{
    if (ptr)
        g_variant_ref_sink(ptr);
    return ptr;
}

template <> void derefGPtr(GVariant* ptr)
{
    if (ptr)
        g_variant_unref(ptr);
}

template <> GVariantBuilder* refGPtr(GVariantBuilder* ptr)
{
    if (ptr)
        g_variant_builder_ref(ptr);
    return ptr;
}

template <> void derefGPtr(GVariantBuilder* ptr)
{
    if (ptr)
        g_variant_builder_unref(ptr);
}

template <> GSource* refGPtr(GSource* ptr)
{
    if (ptr)
        g_source_ref(ptr);
    return ptr;
}

template <> void derefGPtr(GSource* ptr)
{
    if (ptr)
        g_source_unref(ptr);
}

template <> GPtrArray* refGPtr(GPtrArray* ptr)
{
    if (ptr)
        g_ptr_array_ref(ptr);
    return ptr;
}

template <> void derefGPtr(GPtrArray* ptr)
{
    if (ptr)
        g_ptr_array_unref(ptr);
}

template <> GByteArray* refGPtr(GByteArray* ptr)
{
    if (ptr)
        g_byte_array_ref(ptr);
    return ptr;
}

template <> void derefGPtr(GByteArray* ptr)
{
    if (ptr)
        g_byte_array_unref(ptr);
}

template <> GClosure* refGPtr(GClosure* ptr)
{
    if (ptr)
        g_closure_ref(ptr);
    return ptr;
}

template <> void derefGPtr(GClosure* ptr)
{
    if (ptr)
        g_closure_unref(ptr);
}

template <> GRegex* refGPtr(GRegex* ptr)
{
    if (ptr)
        g_regex_ref(ptr);
    return ptr;
}

template <> void derefGPtr(GRegex* ptr)
{
    if (ptr)
        g_regex_unref(ptr);
}

template <> GMappedFile* refGPtr(GMappedFile* ptr)
{
    if (ptr)
        g_mapped_file_ref(ptr);
    return ptr;
}

template <> void derefGPtr(GMappedFile* ptr)
{
    if (ptr)
        g_mapped_file_unref(ptr);
}

template <> GDateTime* refGPtr(GDateTime* ptr)
{
    if (ptr)
        g_date_time_ref(ptr);
    return ptr;
}

template <> void derefGPtr(GDateTime* ptr)
{
    if (ptr)
        g_date_time_unref(ptr);
}

template <> GDBusNodeInfo* refGPtr(GDBusNodeInfo* ptr)
{
    if (ptr)
        g_dbus_node_info_ref(ptr);
    return ptr;
}

template <> void derefGPtr(GDBusNodeInfo* ptr)
{
    if (ptr)
        g_dbus_node_info_unref(ptr);
}

#if HAVE(GURI)
template <> GUri* refGPtr(GUri* ptr)
{
    if (ptr)
        g_uri_ref(ptr);
    return ptr;
}

template <> void derefGPtr(GUri* ptr)
{
    if (ptr)
        g_uri_unref(ptr);
}
#endif

template <>
GArray* refGPtr(GArray* ptr)
{
    if (ptr)
        g_array_ref(ptr);

    return ptr;
}

template <>
void derefGPtr(GArray* ptr)
{
    if (ptr)
        g_array_unref(ptr);
}

} // namespace WTF

#endif // USE(GLIB)
