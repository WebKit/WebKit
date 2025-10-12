/*
 * Copyright (C) 2023 Igalia S.L.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
 * PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "WPECursorTheme.h"

#include <stdio.h>
#include <wtf/FileSystem.h>
#include <wtf/TZoneMallocInlines.h>

namespace WPE {

WTF_MAKE_TZONE_ALLOCATED_IMPL(CursorTheme);

static GUniquePtr<char> cursorsPath(const char* basePath, Vector<GUniquePtr<char>>& inherited)
{
    auto inheritedThemes = [&]() -> GUniquePtr<char*> {
        GUniquePtr<char> index(g_build_filename(basePath, "index.theme", nullptr));
        if (!g_file_test(index.get(), G_FILE_TEST_EXISTS))
            return nullptr;

        GUniquePtr<GKeyFile> keyFile(g_key_file_new());
        if (!g_key_file_load_from_file(keyFile.get(), index.get(), G_KEY_FILE_NONE, nullptr))
            return nullptr;

        return GUniquePtr<char*>(g_key_file_get_string_list(keyFile.get(), "Icon Theme", "Inherits", nullptr, nullptr));
    };

    String pathOfIndex = FileSystem::pathByAppendingComponent(String::fromUTF8(basePath), "index.theme"_s);
    String canonicalPathOfIndex = FileSystem::realPath(pathOfIndex);
    GUniquePtr<char> canonicalDirectoryOfIndex(g_path_get_dirname(canonicalPathOfIndex.utf8().data()));
    const char* actualBasePath = g_file_test(canonicalDirectoryOfIndex.get(), G_FILE_TEST_IS_DIR) ? canonicalDirectoryOfIndex.get() : basePath;
    GUniquePtr<char> baseCursorsPath(g_build_filename(actualBasePath, "cursors", nullptr));

    if (auto inherits = inheritedThemes()) {
        for (unsigned i = 0; inherits.get()[i]; ++i) {
            GUniquePtr<char> parentPath(g_path_get_dirname(actualBasePath));
            GUniquePtr<char> inheritedBasePath(g_build_filename(parentPath.get(), inherits.get()[i], nullptr));
            auto path = cursorsPath(inheritedBasePath.get(), inherited);
            auto exists = path && inherited.containsIf([&](const auto& item) {
                return !g_strcmp0(item.get(), path.get());
            });
            if (path && !exists)
                inherited.append(WTFMove(path));
        }
    }

    if (g_file_test(baseCursorsPath.get(), G_FILE_TEST_IS_DIR))
        return baseCursorsPath;

    return nullptr;
}

static std::unique_ptr<CursorTheme> tryCreateTheme(const char* basePath, uint32_t size)
{
    if (!g_file_test(basePath, G_FILE_TEST_IS_DIR))
        return nullptr;

    Vector<GUniquePtr<char>> inheritedThemes;
    auto path = cursorsPath(basePath, inheritedThemes);
    if (!path && inheritedThemes.isEmpty())
        return nullptr;

    if (!path) {
        // If there's no cursors path, use the first inherited theme.
        path = WTFMove(inheritedThemes[0]);
        inheritedThemes.removeAt(0);
    }

    return makeUnique<CursorTheme>(WTFMove(path), size, WTFMove(inheritedThemes));
}

std::unique_ptr<CursorTheme> CursorTheme::create(const char* name, uint32_t size)
{
    auto tryLoadTheme = [](const char* name, uint32_t size) -> std::unique_ptr<CursorTheme> {
        GUniquePtr<char> path(g_build_filename(g_get_user_data_dir(), "icons", name, nullptr));
        if (auto theme = tryCreateTheme(path.get(), size))
            return theme;

        path.reset(g_build_filename(g_get_home_dir(), ".icons", name, nullptr));
        if (auto theme = tryCreateTheme(path.get(), size))
            return theme;

        auto* dataDirs = g_get_system_data_dirs();
        for (unsigned i = 0; dataDirs[i]; ++i) {
            path.reset(g_build_filename(dataDirs[i], "icons", name, nullptr));
            if (auto theme = tryCreateTheme(path.get(), size))
                return theme;
        }

        return nullptr;
    };

    bool isFallback = false;
    for (const char* themeName : { name, "default", "Adwaita" }) {
        if (isFallback && (themeName == name || !g_strcmp0(themeName, name)))
            continue;

        if (auto theme = tryLoadTheme(themeName, size))
            return theme;

        isFallback = true;
    }

    // FIXME: add builtin cursors to always have a valid fallback.
    g_warning("Could not create cursor theme for '%s'", name);
    return nullptr;
}

std::unique_ptr<CursorTheme> CursorTheme::create()
{
    return create("default", 24);
}

CursorTheme::CursorTheme(GUniquePtr<char>&& path, uint32_t size, Vector<GUniquePtr<char>>&& inherited)
    : m_path(WTFMove(path))
    , m_size(size)
    , m_inherited(WTFMove(inherited))
{
}

#define XCURSOR_MAGIC 0x72756358
#define XCURSOR_IMAGE_TYPE 0xfffd0002
#define XCURSOR_IMAGE_MAX_SIZE 0x7fff // 32767x32767 max cursor size.

struct XcusorContent {
    uint32_t type { 0 };
    uint32_t subtype { 0 };
    uint32_t position { 0 };
};

struct XcursorHeader {
    uint32_t magic { 0 };
    uint32_t length { 0 };
    uint32_t version { 0 };
    Vector<XcusorContent> contents;
    uint32_t imageSize { 0 };
    uint32_t imageCount { 0 };
};

struct XcursorChunkHeader {
    uint32_t length { 0 };
    uint32_t type { 0 };
    uint32_t subtype { 0 };
    uint32_t version { 0 };
};

static bool readUint32(FILE* file, uint32_t* value)
{
    unsigned char bytes[4];
    if (fread(&bytes, 1, 4, file) != 4)
        return false;

    *value = ((bytes[0] << 0) | (bytes[1] << 8) | (bytes[2] << 16) | (bytes[3] << 24));
    return true;
}

#define distance(x, y) (x > y ? x - y : y - x)

static std::optional<XcursorHeader> readHeader(FILE* file, uint32_t size)
{
    XcursorHeader header;
    if (!readUint32(file, &header.magic) || header.magic != XCURSOR_MAGIC)
        return std::nullopt;
    if (!readUint32(file, &header.length))
        return std::nullopt;
    if (!readUint32(file, &header.version))
        return std::nullopt;
    uint32_t tocLength;
    if (!readUint32(file, &tocLength) || tocLength > std::numeric_limits<uint16_t>::max())
        return std::nullopt;
    if (auto bytesToSkip = header.length - (4 * 4)) {
        if (fseek(file, bytesToSkip, SEEK_CUR) == EOF)
            return std::nullopt;
    }

    header.contents.grow(tocLength);
    for (auto& entry : header.contents) {
        if (!readUint32(file, &entry.type))
            return std::nullopt;
        if (!readUint32(file, &entry.subtype))
            return std::nullopt;
        if (!readUint32(file, &entry.position))
            return std::nullopt;
        if (entry.type != XCURSOR_IMAGE_TYPE)
            continue;

        if (!header.imageSize || distance(entry.subtype, size) < distance(header.imageSize, size)) {
            header.imageSize = entry.subtype;
            header.imageCount = 1;
        } else if (entry.subtype == header.imageSize)
            header.imageCount++;
    }

    if (!header.imageSize || !header.imageCount)
        return std::nullopt;

    return header;
}

static std::optional<XcursorChunkHeader> readChunkHeader(FILE* file, const XcusorContent& entry)
{
    if (fseek(file, entry.position, SEEK_SET) == EOF)
        return std::nullopt;

    XcursorChunkHeader chunkHeader;
    if (!readUint32(file, &chunkHeader.length))
        return std::nullopt;
    if (!readUint32(file, &chunkHeader.type) || chunkHeader.type != entry.type)
        return std::nullopt;
    if (!readUint32(file, &chunkHeader.subtype) || chunkHeader.subtype != entry.subtype)
        return std::nullopt;
    if (!readUint32(file, &chunkHeader.version))
        return std::nullopt;
    return chunkHeader;
}

static std::optional<CursorTheme::CursorImage> readImage(FILE* file, const XcusorContent& entry)
{
    auto chunkHeader = readChunkHeader(file, entry);
    if (!chunkHeader)
        return std::nullopt;

    CursorTheme::CursorImage image;
    if (!readUint32(file, &image.width) || !image.width || image.width > XCURSOR_IMAGE_MAX_SIZE)
        return std::nullopt;
    if (!readUint32(file, &image.height) || !image.height || image.height > XCURSOR_IMAGE_MAX_SIZE)
        return std::nullopt;
    if (!readUint32(file, &image.hotspotX) || image.hotspotX > image.width)
        return std::nullopt;
    if (!readUint32(file, &image.hotspotY) || image.hotspotY > image.height)
        return std::nullopt;
    if (!readUint32(file, &image.delay))
        return std::nullopt;

    auto imageSize = image.width * image.height;
    image.pixels.grow(imageSize);
    auto* pixels = image.pixels.mutableSpan().data();
    while (imageSize--) {
        if (!readUint32(file, pixels))
            return std::nullopt;
        pixels++;
    }

    return image;
}

Vector<CursorTheme::CursorImage> CursorTheme::loadCursor(const char* name, uint32_t size, std::optional<uint32_t> maxImages)
{
    GUniquePtr<char> path(g_build_filename(m_path.get(), name, nullptr));
    if (!g_file_test(path.get(), G_FILE_TEST_EXISTS)) {
        path = nullptr;
        for (auto& theme : m_inherited) {
            path.reset(g_build_filename(theme.get(), name, nullptr));
            if (g_file_test(path.get(), G_FILE_TEST_EXISTS))
                break;
            path = nullptr;
        }
    }

    if (!path)
        return { };

    FILE* file = fopen(path.get(), "r");
    if (!file)
        return { };

    auto header = readHeader(file, size);
    if (!header) {
        fclose(file);
        return { };
    }

    header->imageCount = maxImages.value_or(header->imageCount);

    Vector<CursorImage> cursorImages;
    for (auto& entry : header->contents) {
        if (entry.type != XCURSOR_IMAGE_TYPE)
            continue;

        if (entry.subtype != header->imageSize)
            continue;

        auto image = readImage(file, entry);
        if (!image)
            break;

        cursorImages.append(WTFMove(*image));
        if (cursorImages.size() == header->imageCount)
            break;
    }

    fclose(file);

    return cursorImages;
}

} // namespace WPE
