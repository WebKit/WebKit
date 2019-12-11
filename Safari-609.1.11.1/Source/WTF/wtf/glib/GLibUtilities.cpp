/*
 * Copyright (C) 2010 Igalia, S.L.
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Library General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Library General Public License for more details.
 *
 *  You should have received a copy of the GNU Library General Public License
 *  along with this library; see the file COPYING.LIB.  If not, write to
 *  the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 *  Boston, MA 02110-1301, USA.
 */

#include "config.h"
#include <wtf/glib/GLibUtilities.h>

#include <glib.h>
#include <wtf/glib/GUniquePtr.h>

#if OS(WINDOWS)
#include <windows.h>
#include <wtf/text/WTFString.h>
#else
#include <limits.h>
#include <unistd.h>
#endif

#if OS(LINUX)
CString getCurrentExecutablePath()
{
    static char readLinkBuffer[PATH_MAX];
    ssize_t result = readlink("/proc/self/exe", readLinkBuffer, PATH_MAX);
    if (result == -1)
        return CString();
    return CString(readLinkBuffer, result);
}
#elif OS(HURD)
CString getCurrentExecutablePath()
{
    return CString();
}
#elif OS(UNIX)
CString getCurrentExecutablePath()
{
    static char readLinkBuffer[PATH_MAX];
    ssize_t result = readlink("/proc/curproc/file", readLinkBuffer, PATH_MAX);
    if (result == -1)
        return CString();
    return CString(readLinkBuffer, result);
}
#elif OS(WINDOWS)
CString getCurrentExecutablePath()
{
    static WCHAR buffer[MAX_PATH];
    DWORD length = GetModuleFileNameW(0, buffer, MAX_PATH);
    if (!length || (length == MAX_PATH && GetLastError() == ERROR_INSUFFICIENT_BUFFER))
        return CString();

    String path(buffer, length);
    return path.utf8();
}
#endif

CString getCurrentExecutableName()
{
    auto executablePath = getCurrentExecutablePath();
    if (!executablePath.isNull()) {
        GUniquePtr<char> basename(g_path_get_basename(executablePath.data()));
        return basename.get();
    }

    return g_get_prgname();
}

CString enumToString(GType type, guint value)
{
#if GLIB_CHECK_VERSION(2, 54, 0)
    GUniquePtr<char> result(g_enum_to_string(type, value));
    return result.get();
#else
    GEnumClass* enumClass = reinterpret_cast<GEnumClass*>(g_type_class_ref(type));
    GEnumValue* enumValue = g_enum_get_value(enumClass, value);
    char* representation = enumValue ? g_strdup(enumValue->value_nick) : nullptr;
    g_type_class_unref(enumClass);
    GUniquePtr<char> result(representation);
    return result.get();
#endif
}
