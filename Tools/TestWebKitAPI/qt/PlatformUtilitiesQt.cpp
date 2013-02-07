/*
 * Copyright (C) 2013 Digia Plc and/or its subsidiary(-ies).
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
 *
 */

#include "config.h"
#include "PlatformUtilities.h"

#include <QCoreApplication>

#include <unistd.h>

namespace TestWebKitAPI {
namespace Util {

void run(bool* done)
{
    while (!*done)
        QCoreApplication::processEvents();
}

void sleep(double seconds)
{
    usleep(seconds * 1000000);
}

WKStringRef createInjectedBundlePath()
{
    // ### FIXME.
    return WKStringCreateWithUTF8CString("");
}

WKURLRef createURLForResource(const char* resource, const char* extension)
{
    // ### FIXME.
    return WKURLCreateWithUTF8CString("");
}

WKURLRef URLForNonExistentResource()
{
    return WKURLCreateWithUTF8CString("file:///does-not-exist.html");
}

} // namespace Util
} // namespace TestWebKitAPI
