/*
 * Copyright (C) 2024 Igalia S.L.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include "config.h"
#include <wtf/glib/GSpanExtras.h>

#include <wtf/StdLibExtras.h>
#include <wtf/text/CStringView.h>

namespace WTF {

Expected<GMallocSpan<char>, GUniquePtr<GError>> gFileGetContents(CStringView path)
{
    char* contents;
    gsize length;
    GUniqueOutPtr<GError> error;
    if (!g_file_get_contents(path.utf8(), &contents, &length, &error.outPtr()))
        return makeUnexpected(GUniquePtr<GError>(error.release()));

    return adoptGMallocSpan(unsafeMakeSpan(contents, length));
}

Expected<GMallocSpan<char*, GMallocStrv>, GUniquePtr<GError>> gKeyFileGetKeys(GKeyFile* keyFile, CStringView groupName)
{
    ASSERT(keyFile);
    ASSERT(groupName);

    size_t keyCount = 0;
    GUniqueOutPtr<GError> error;
    char** keys = g_key_file_get_keys(keyFile, groupName.utf8(), &keyCount, &error.outPtr());
    if (error)
        return makeUnexpected(GUniquePtr<GError>(error.release()));
    return adoptGMallocSpan<char*, GMallocStrv>(unsafeMakeSpan(keys, keyCount));
}

GMallocSpan<GParamSpec*> gObjectClassGetProperties(GObjectClass* objectClass)
{
    ASSERT(objectClass);

    unsigned propertyCount = 0;
    GParamSpec** properties = g_object_class_list_properties(objectClass, &propertyCount);
    return adoptGMallocSpan(unsafeMakeSpan(properties, propertyCount));
}

GMallocSpan<const char*> gVariantGetStrv(const GRefPtr<GVariant>& variant)
{
    ASSERT(variant);

    size_t stringCount = 0;
    const char** strings = g_variant_get_strv(variant.get(), &stringCount);
    return adoptGMallocSpan(unsafeMakeSpan(strings, stringCount));
}

} // namespace WTF
