/*
 * Copyright (C) 2007, 2008, 2009 Apple Inc. All rights reserved.
 * Copyright (C) 2009 Torch Mobile Inc. All rights reserved.
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
#include "FontCustomPlatformData.h"

#include "Base64.h"
#include "CachedFont.h"
#include "FontPlatformData.h"
#include <wtf/RandomNumber.h>

namespace WebCore {

static CustomFontCache* g_customFontCache = 0;

bool renameFont(SharedBuffer* fontData, const String& fontName);

void setCustomFontCache(CustomFontCache* cache)
{
    g_customFontCache = cache;
}

FontCustomPlatformData::~FontCustomPlatformData()
{
    if (g_customFontCache && !m_name.isEmpty())
        g_customFontCache->unregisterFont(m_name);
}

FontPlatformData FontCustomPlatformData::fontPlatformData(int size, bool bold, bool italic, FontRenderingMode renderingMode)
{
    FontDescription fontDesc;
    fontDesc.setComputedSize(size);
    fontDesc.setSpecifiedSize(size);
    fontDesc.setItalic(italic);
    fontDesc.setWeight(bold ? FontWeightBold : FontWeightNormal);
    return FontPlatformData(fontDesc, m_name, false);
}

// Creates a unique and unpredictable font name, in order to avoid collisions and to
// not allow access from CSS.
static String createUniqueFontName()
{
    Vector<char> fontUuid(sizeof(GUID));

    unsigned int* ptr = reinterpret_cast<unsigned int*>(fontUuid.data());
    for (int i = 0; i < sizeof(GUID) / sizeof(int) ; ++i)
        *(ptr + i) = static_cast<unsigned int>(WTF::randomNumber() * (std::numeric_limits<unsigned>::max() + 1.0));

    Vector<char> fontNameVector;
    base64Encode(fontUuid, fontNameVector);
    ASSERT(fontNameVector.size() < LF_FACESIZE);
    String fontName(fontNameVector.data(), fontNameVector.size());
    return fontName.replace('/', '_');
}

FontCustomPlatformData* createFontCustomPlatformData(const SharedBuffer* buffer)
{
    if (g_customFontCache) {
        String fontName = createUniqueFontName();
        RefPtr<SharedBuffer> localBuffer = SharedBuffer::create(buffer->data(), buffer->size());
        if (renameFont(localBuffer.get(), fontName) && g_customFontCache->registerFont(fontName, localBuffer.get()))
            return new FontCustomPlatformData(fontName);
    }
    return 0;
}

}
