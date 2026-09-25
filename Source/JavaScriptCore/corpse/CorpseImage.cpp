/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
 * Copyright (C) 2026 Igalia S.L.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "CorpseImage.h"

#if ENABLE(MYA)

#include "CorpseError.h"
#include "CorpseLimits.h"
#include "CorpseProcess.h"
#include "CorpseSnapshot.h"

#if OS(DARWIN)
#include <mach-o/dyld_images.h>
#include <mach/mach.h>
#include <mach/mach_error.h>
#include <mach/task_info.h>
#else
#include <algorithm>
#include <array>
#include <errno.h>
#include <fcntl.h>
#include <optional>
#include <string_view>
#include <unistd.h>
#include <wtf/Scope.h>
#include <wtf/StdLibExtras.h>
#include <wtf/text/MakeString.h>
#include <wtf/text/StringToIntegerConversion.h>
#include <wtf/text/StringView.h>
#endif

namespace JSC {
namespace Corpse {

#if OS(DARWIN)

Vector<Image> Image::platformCollect(const Snapshot& snapshot)
{
    Vector<Image> result;

    task_dyld_info_data_t dyldInfo;
    mach_msg_type_number_t count = TASK_DYLD_INFO_COUNT;
    kern_return_t kr = task_info(snapshot.corpsePort(), TASK_DYLD_INFO, reinterpret_cast<task_info_t>(&dyldInfo), &count);
    if (kr != KERN_SUCCESS) {
        CORPSE_REPORT("Could not read dyld information: %s (0x%x)", mach_error_string(kr), kr);
        return result;
    }

    Address allImageInfosAddress { dyldInfo.all_image_info_addr };
    if (!allImageInfosAddress) {
        CORPSE_REPORT("dyld reports no image list");
        return result;
    }
    auto allImages = snapshot.read<dyld_all_image_infos>(allImageInfosAddress);
    if (!allImages) {
        CORPSE_REPORT("Could not read dyld_all_image_infos at 0x%llx", allImageInfosAddress.toTargetVMAddress());
        return result;
    }

    Address arrayAddress = Address { allImages->infoArray }.stripped();
    uint32_t imageCount = allImages->infoArrayCount;
    if (!arrayAddress || !imageCount) {
        CORPSE_REPORT("dyld_all_image_infos v%u at 0x%llx lists no images",
            allImages->version, allImageInfosAddress.toTargetVMAddress());
        return result;
    }
    // Each image below costs a Mach round-trip and two buffer reads, so an
    // implausible count is a lot of work to be talked into doing.
    if (imageCount > maxImageCount) {
        CORPSE_REPORT("%u images is too many to be a real image list", imageCount);
        return result;
    }

    result.reserveCapacity(imageCount);
    for (uint32_t i = 0; i < imageCount; ++i) {
        auto info = snapshot.read<dyld_image_info>(arrayAddress + static_cast<uint64_t>(i) * sizeof(dyld_image_info));
        if (!info) {
            Diagnostics::count("unreadable image entries"_s);
            continue;
        }
        Diagnostics::count("image entries read"_s);
        auto imageAddress = Address(info->imageLoadAddress).stripped();
        auto path = snapshot.copyCString(Address { info->imageFilePath }.stripped(), maxPathLength);
        if (!path) {
            CORPSE_REPORT("Could not read the path of the image at 0x%llx", imageAddress.toTargetVMAddress());
            return { };
        }
        result.append(Image { WTF::move(*path), imageAddress });
    }
    return result;
}

#else

// One line of /proc/<pid>/maps: "start-end perms offset dev inode path", with
// the path column padded by spaces and absent for an anonymous mapping.
struct MapsLine {
    uint64_t start;
    uint64_t fileOffset;
    std::string_view path;
};

static std::string_view takeWord(std::string_view& line)
{
    size_t end = line.find(' ');
    std::string_view word = line.substr(0, end);
    line = end == std::string_view::npos ? std::string_view { } : line.substr(end + 1);
    return word;
}

static std::optional<uint64_t> parseHex(std::string_view text)
{
    return parseInteger<uint64_t>(StringView { std::span<const char> { text } }, 16);
}

static std::optional<MapsLine> parseMapsLine(std::string_view line)
{
    std::string_view range = takeWord(line);
    takeWord(line); // Permissions.
    std::string_view offset = takeWord(line);
    takeWord(line); // Device.
    takeWord(line); // Inode.

    auto start = parseHex(range.substr(0, range.find('-')));
    auto fileOffset = parseHex(offset);
    if (!start || !fileOffset)
        return std::nullopt;

    size_t pathStart = line.find('/');
    std::string_view path = pathStart == std::string_view::npos ? std::string_view { } : line.substr(pathStart);
    return MapsLine { *start, *fileOffset, path };
}

static std::optional<Vector<uint8_t>> readEntireFile(const CString& path)
{
    int fd = open(path.data(), O_RDONLY | O_CLOEXEC);
    if (fd < 0)
        return std::nullopt;
    auto closeFile = makeScopeExit([&] {
        close(fd);
    });

    // procfs reports every file as empty, so it has to be read to its end.
    Vector<uint8_t> contents;
    std::array<uint8_t, 16 * KB> chunk;
    while (true) {
        ssize_t got = read(fd, chunk.data(), chunk.size());
        if (got < 0 && errno == EINTR)
            continue;
        if (got < 0)
            return std::nullopt;
        if (!got)
            return contents;
        contents.append(std::span { chunk }.first(static_cast<size_t>(got)));
    }
}

// The lowest mapping of each file at file offset zero is where its header
// landed; that is what /proc/<pid>/maps has in place of a loader's image list.
Vector<Image> Image::platformCollect(const Snapshot& snapshot)
{
    Vector<Image> result;

    auto maps = readEntireFile(makeString("/proc/"_s, snapshot.process()->pid(), "/maps"_s).utf8());
    if (!maps || maps->isEmpty()) {
        CORPSE_REPORT("Could not read the memory map");
        return result;
    }

    std::string_view text { byteCast<char>(maps->span()) };
    while (!text.empty()) {
        size_t newline = text.find('\n');
        std::string_view line = text.substr(0, newline);
        text = newline == std::string_view::npos ? std::string_view { } : text.substr(newline + 1);

        Diagnostics::count("mappings"_s);
        auto mapping = parseMapsLine(line);
        if (!mapping || mapping->fileOffset || mapping->path.empty() || mapping->path.size() > maxPathLength)
            continue;

        size_t index = result.findIf([&](const Image& image) {
            return std::string_view { image.m_path.span() } == mapping->path;
        });
        if (index != notFound) {
            result[index].m_loadAddress = std::min(result[index].m_loadAddress, Address { mapping->start });
            continue;
        }
        result.append(Image { UTF8CString(byteCast<char8_t>(std::span { mapping->path })), Address { mapping->start } });
    }

    // Data files, like the locale archive, are mapped too; only an ELF object
    // has a header where it is loaded.
    result.removeAllMatching([&](const Image& image) {
        std::array<uint8_t, 4> magic;
        std::span<uint8_t> got = snapshot.read(image.loadAddress(), std::span { magic });
        return got.empty() || !std::ranges::equal(got, std::array<uint8_t, 4> { 0x7f, 'E', 'L', 'F' });
    });
    Diagnostics::count("images with an ELF header"_s, result.size());
    return result;
}

#endif // OS(DARWIN)

Vector<Image> Image::collect(const Snapshot& snapshot)
{
    if (!snapshot.isValid()) {
        CORPSE_REPORT("Cannot read images from an invalid snapshot");
        return { };
    }
    CORPSE_DIAGNOSTICS("listing the images of pid %d", static_cast<int>(snapshot.process()->pid()));
    return platformCollect(snapshot);
}

} // namespace Corpse
} // namespace JSC

#endif // ENABLE(MYA)
