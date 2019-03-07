/*
* Copyright (C) 2006, 2007, 2008 Apple Inc. All rights reserved.
* Copyright (C) 2007-2009 Torch Mobile, Inc.
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
#include "LocalWindowsContext.h"
#include <windows.h>
#include <wtf/text/WTFString.h>

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

// FIXME: Move the code to ChromeClient::iconForFiles().
RefPtr<Icon> Icon::createIconForFiles(const Vector<String>& filenames)
{
    if (filenames.isEmpty())
        return 0;

    if (filenames.size() == 1) {
        SHFILEINFO sfi;
        memset(&sfi, 0, sizeof(sfi));

        String tmpFilename = filenames[0];
        if (!SHGetFileInfo(tmpFilename.wideCharacters().data(), 0, &sfi, sizeof(sfi), SHGFI_ICON | SHGFI_SHELLICONSIZE | SHGFI_SMALLICON))
            return 0;

        return adoptRef(new Icon(sfi.hIcon));
    }

    WCHAR buffer[MAX_PATH];
    UINT length = ::GetSystemDirectoryW(buffer, WTF_ARRAY_LENGTH(buffer));
    if (!length)
        return 0;

    if (wcscat_s(buffer, L"\\shell32.dll"))
        return 0;

    HICON hIcon;
    if (!::ExtractIconExW(buffer, shell32MultipleFileIconIndex, 0, &hIcon, 1))
        return 0;
    return adoptRef(new Icon(hIcon));
}

void Icon::paint(GraphicsContext& context, const FloatRect& r)
{
    if (context.paintingDisabled())
        return;

    LocalWindowsContext windowContext(context, enclosingIntRect(r));
    DrawIconEx(windowContext.hdc(), r.x(), r.y(), m_hIcon, r.width(), r.height(), 0, 0, DI_NORMAL);
}

}
