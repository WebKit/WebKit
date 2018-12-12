/*
 * Copyright (C) 2008 Alp Toker <alp@atoker.com>
 * Copyright (C) 2010 Igalia S.L.
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

#include "FontPlatformData.h"
#include "SharedBuffer.h"
#include <cairo-ft.h>
#include <cairo.h>
#include <ft2build.h>
#include FT_MODULE_H

namespace WebCore {

static void releaseCustomFontData(void* data)
{
    static_cast<SharedBuffer*>(data)->deref();
}

FontCustomPlatformData::FontCustomPlatformData(FT_Face freeTypeFace, SharedBuffer& buffer)
    : m_fontFace(cairo_ft_font_face_create_for_ft_face(freeTypeFace, FT_LOAD_DEFAULT))
{
    buffer.ref(); // This is balanced by the buffer->deref() in releaseCustomFontData.
    static cairo_user_data_key_t bufferKey;
    cairo_font_face_set_user_data(m_fontFace, &bufferKey, &buffer,
         static_cast<cairo_destroy_func_t>(releaseCustomFontData));

    // Cairo doesn't do FreeType reference counting, so we need to ensure that when
    // this cairo_font_face_t is destroyed, it cleans up the FreeType face as well.
    static cairo_user_data_key_t freeTypeFaceKey;
    cairo_font_face_set_user_data(m_fontFace, &freeTypeFaceKey, freeTypeFace,
        reinterpret_cast<cairo_destroy_func_t>(reinterpret_cast<void(*)(void)>(FT_Done_Face)));
}

FontCustomPlatformData::~FontCustomPlatformData()
{
    cairo_font_face_destroy(m_fontFace);
}

FontPlatformData FontCustomPlatformData::fontPlatformData(const FontDescription& description, bool bold, bool italic)
{
    return FontPlatformData(m_fontFace, description, bold, italic);
}

static bool initializeFreeTypeLibrary(FT_Library& library)
{
    // https://www.freetype.org/freetype2/docs/design/design-4.html
    // https://lists.nongnu.org/archive/html/freetype-devel/2004-10/msg00022.html

    FT_Memory memory = bitwise_cast<FT_Memory>(ft_smalloc(sizeof(*memory)));
    if (!memory)
        return false;

    memory->user = nullptr;
    memory->alloc = [](FT_Memory, long size) -> void* {
        return fastMalloc(size);
    };
    memory->free = [](FT_Memory, void* block) -> void {
        fastFree(block);
    };
    memory->realloc = [](FT_Memory, long, long newSize, void* block) -> void* {
        return fastRealloc(block, newSize);
    };

    if (FT_New_Library(memory, &library)) {
        ft_sfree(memory);
        return false;
    }

    FT_Add_Default_Modules(library);
    return true;
}

std::unique_ptr<FontCustomPlatformData> createFontCustomPlatformData(SharedBuffer& buffer, const String&)
{
    static FT_Library library;
    if (!library && !initializeFreeTypeLibrary(library)) {
        library = nullptr;
        return nullptr;
    }

    FT_Face freeTypeFace;
    if (FT_New_Memory_Face(library, reinterpret_cast<const FT_Byte*>(buffer.data()), buffer.size(), 0, &freeTypeFace))
        return nullptr;
    return std::make_unique<FontCustomPlatformData>(freeTypeFace, buffer);
}

bool FontCustomPlatformData::supportsFormat(const String& format)
{
    return equalLettersIgnoringASCIICase(format, "truetype")
        || equalLettersIgnoringASCIICase(format, "opentype")
#if USE(WOFF2)
        || equalLettersIgnoringASCIICase(format, "woff2")
#endif
        || equalLettersIgnoringASCIICase(format, "woff");
}

}
