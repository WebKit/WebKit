/*
* Copyright (C) 2006, 2007, 2008 Apple Inc. All rights reserved.
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
#include "Icon.h"

#include "GraphicsContext.h"
#include "PlatformString.h"
#include <tchar.h>
#include <windows.h>

namespace WebCore {

static const int shell32MultipleFileIconIndex = 54;

Icon::Icon(HICON icon)
    : m_hIcon(icon)
{
    ASSERT(icon);
}

Icon::~Icon()
{
    DestroyIcon(m_hIcon);
}

PassRefPtr<Icon> Icon::createIconForFile(const String& filename)
{
    SHFILEINFO sfi;
    memset(&sfi, 0, sizeof(sfi));

    String tmpFilename = filename;
    if (!SHGetFileInfo(tmpFilename.charactersWithNullTermination(), 0, &sfi, sizeof(sfi), SHGFI_ICON | SHGFI_SHELLICONSIZE | SHGFI_SMALLICON))
        return 0;

    return adoptRef(new Icon(sfi.hIcon));
}

PassRefPtr<Icon> Icon::createIconForFiles(const Vector<String>&)
{
    TCHAR buffer[MAX_PATH];    
    UINT length = ::GetSystemDirectory(buffer, ARRAYSIZE(buffer));
    if (!length)
        return 0;
    
    if (_tcscat_s(buffer, TEXT("\\shell32.dll")))
        return 0;

    HICON hIcon;
    if (!::ExtractIconEx(buffer, shell32MultipleFileIconIndex, 0, &hIcon, 1))
        return 0;
    return adoptRef(new Icon(hIcon));
}

void Icon::paint(GraphicsContext* context, const IntRect& r)
{
    if (context->paintingDisabled())
        return;

    HDC hdc = context->getWindowsContext(r);

    DrawIconEx(hdc, r.x(), r.y(), m_hIcon, r.width(), r.height(), 0, 0, DI_NORMAL);

    context->releaseWindowsContext(hdc, r);
}

}
