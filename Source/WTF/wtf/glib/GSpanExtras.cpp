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

namespace WTF {

GOwningSpan<const char*> gKeyFileGetKeys(GKeyFile* keyFile, const char* groupName, GUniqueOutPtr<GError>& error)
{
    ASSERT(keyFile);
    ASSERT(groupName);

    size_t keyCount = 0;
    char** keys = g_key_file_get_keys(keyFile, groupName, &keyCount, &error.outPtr());
    return GOwningSpan { const_cast<const char**>(keys), keyCount };
}

GOwningSpan<GParamSpec*> gObjectClassGetProperties(GObjectClass* objectClass)
{
    ASSERT(objectClass);

    unsigned propertyCount = 0;
    GParamSpec** properties = g_object_class_list_properties(objectClass, &propertyCount);
    return GOwningSpan { properties, propertyCount };
}

} // namespace WTF
