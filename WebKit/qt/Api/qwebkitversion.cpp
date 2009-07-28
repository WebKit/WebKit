/*
    Copyright (C) 2009 Robert Hogan <robert@roberthogan.net>

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Library General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Library General Public License for more details.

    You should have received a copy of the GNU Library General Public License
    along with this library; see the file COPYING.LIB.  If not, write to
    the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
    Boston, MA 02110-1301, USA.
*/

#include "config.h"
#include <qwebkitversion.h>
#include <WebKitVersion.h>

/*!

    Returns the version number of WebKit at run-time as a string (for
    example, "531.3"). This is the version of WebKit the application
    was compiled against.

*/
QString qWebKitVersion()
{
    return QString("%1.%2").arg(WEBKIT_MAJOR_VERSION).arg(WEBKIT_MINOR_VERSION);
}

/*!

    Returns the 'major' version number of WebKit at run-time as an integer
    (for example, 531). This is the version of WebKit the application
    was compiled against.

*/
int qWebKitMajorVersion()
{
    return WEBKIT_MAJOR_VERSION;
}

/*!

    Returns the 'minor' version number of WebKit at run-time as an integer
    (for example, 3). This is the version of WebKit the application
    was compiled against.

*/
int qWebKitMinorVersion()
{
    return WEBKIT_MINOR_VERSION;
}
