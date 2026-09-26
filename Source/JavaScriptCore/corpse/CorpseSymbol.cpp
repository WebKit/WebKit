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
#include "CorpseSymbol.h"

#if ENABLE(MYA)

#include "CorpseError.h"
#include "CorpseExportsTrie.h"
#include "CorpseImage.h"
#include "CorpseLimits.h"
#include "CorpseSnapshot.h"

#if OS(DARWIN)
#include <mach-o/loader.h>
#endif
#include <optional>
#include <span>
#include <string.h>
#include <string_view>
#include <type_traits>
#include <wtf/StdLibExtras.h>
#include <wtf/TZoneMallocInlines.h>
#include <wtf/Vector.h>
#include <wtf/text/StringCommon.h>

namespace JSC {
namespace Corpse {

WTF_MAKE_TZONE_ALLOCATED_IMPL(Symbol);

#if OS(DARWIN)

// It is assumed that this corpse analysis library is built with the same SDK targeting
// the same OS that the corpse binary is built for. While the corpse gives us the data
// to inspect, it does not provide the format. Hence, we need to rely on the invariant
// that this analysis library is built with the same understanding of the same data
// format used in the corpse. This is how we can walk and interpret the corpse's
// dyld exports trie and get addresses of symbols.

namespace {

template<typename T>
std::optional<T> readCommand(std::span<const uint8_t> commands, size_t offset)
{
    static_assert(std::is_trivially_copyable_v<T>);
    // Compared against what is left of the buffer rather than by forming
    // offset + sizeof(T), which could wrap and pass a direct comparison. The
    // first clause is what makes the subtraction safe.
    if (offset > commands.size() || commands.size() - offset < sizeof(T))
        return std::nullopt;

    T value;
    memcpySpan(asMutableByteSpan(value), commands.subspan(offset, sizeof(T)));
    return value;
}

bool segmentNameIs(const char (&name)[16], std::string_view expected)
{
    // A segment name fills the whole array when it is exactly 16 characters, in
    // which case it has no terminator.
    std::span<const char> span { name };
    return std::string_view(span.first(strlenSpan(span))) == expected;
}

} // anonymous namespace

bool Symbol::hasReadBudget(size_t length)
{
    if (length > m_readBudget) {
        Diagnostics::count("images skipped for the read budget"_s);
        return false;
    }
    m_readBudget -= length;
    return true;
}

// Resolves `name` in the one image loaded at `imageAddress`, via its exports
// trie. Returns a null address if this image does not export it.
Address Symbol::resolveInImage(const Snapshot& snapshot, Address imageAddress, std::string_view name)
{
    auto header = snapshot.read<mach_header_64>(imageAddress);
    if (!header || header->magic != MH_MAGIC_64) {
        Diagnostics::count("unreadable image headers"_s);
        return { };
    }
    Diagnostics::count("image headers read"_s);
    if (header->flags & MH_DYLIB_IN_CACHE)
        Diagnostics::count("images in the shared cache"_s);

    if (header->sizeofcmds > maxLoadCommandsSize) {
        Diagnostics::count("implausible load command sizes"_s);
        return { };
    }
    if (!hasReadBudget(header->sizeofcmds))
        return { };
    auto commandsBuffer = snapshot.copyBytes(imageAddress + sizeof(mach_header_64), header->sizeofcmds);
    if (!commandsBuffer) {
        Diagnostics::count("unreadable load commands"_s);
        return { };
    }
    std::span<const uint8_t> commands = commandsBuffer->span();

    std::optional<uint64_t> textVMAddress;
    std::optional<uint64_t> linkeditVMAddress;
    std::optional<uint64_t> linkeditFileOffset;
    std::optional<uint64_t> linkeditFileSize;
    uint32_t exportOffset = 0;
    uint32_t exportSize = 0;

    size_t offset = 0;
    for (uint32_t i = 0; i < header->ncmds; ++i) {
        auto command = readCommand<load_command>(commands, offset);
        // cmdsize is compared against what is left of the blob rather than by
        // forming offset + cmdsize, which could wrap and pass a direct
        // comparison. The subtraction is safe only because a successful
        // readCommand has already established that offset is within the blob,
        // so the clauses have to stay in this order.
        if (!command || command->cmdsize < sizeof(load_command) || command->cmdsize > commands.size() - offset)
            break;

        // Each case below re-reads `offset` as the larger struct the command
        // claims to be. cmdsize has to cover that struct too: a command that
        // declares itself smaller is malformed, and reading it anyway would take
        // the fields that follow it as its own.
        switch (command->cmd) {
        case LC_SEGMENT_64: {
            if (command->cmdsize < sizeof(segment_command_64))
                break;
            auto segment = readCommand<segment_command_64>(commands, offset);
            if (!segment)
                break;
            if (segmentNameIs(segment->segname, SEG_TEXT))
                textVMAddress = segment->vmaddr;
            else if (segmentNameIs(segment->segname, SEG_LINKEDIT)) {
                linkeditVMAddress = segment->vmaddr;
                linkeditFileOffset = segment->fileoff;
                linkeditFileSize = segment->filesize;
            }
            break;
        }
        case LC_DYLD_INFO:
        case LC_DYLD_INFO_ONLY: {
            if (command->cmdsize < sizeof(dyld_info_command))
                break;
            auto info = readCommand<dyld_info_command>(commands, offset);
            if (!info)
                break;
            exportOffset = info->export_off;
            exportSize = info->export_size;
            break;
        }
        case LC_DYLD_EXPORTS_TRIE: {
            if (command->cmdsize < sizeof(linkedit_data_command))
                break;
            auto data = readCommand<linkedit_data_command>(commands, offset);
            if (!data)
                break;
            exportOffset = data->dataoff;
            exportSize = data->datasize;
            break;
        }
        default:
            break;
        }
        offset += command->cmdsize;
    }

    if (!textVMAddress || !linkeditVMAddress || !linkeditFileOffset || !linkeditFileSize || !exportSize) {
        Diagnostics::count("images without an exports trie"_s);
        return { };
    }
    if (exportSize > maxExportsTrieSize) {
        Diagnostics::count("implausible exports trie sizes"_s);
        return { };
    }

    // The trie is file-backed data living inside __LINKEDIT. So, exportOffset cannot
    // be less than the start of __LINKEDIT, cannot exceed the end of __LINKEDIT, and
    // the whole trie must fit inside it.
    if (exportOffset < *linkeditFileOffset) {
        Diagnostics::count("exports tries outside __LINKEDIT"_s);
        return { };
    }
    uint64_t trieSegmentOffset = exportOffset - *linkeditFileOffset;
    if (trieSegmentOffset > *linkeditFileSize || exportSize > *linkeditFileSize - trieSegmentOffset) {
        Diagnostics::count("exports tries outside __LINKEDIT"_s);
        return { };
    }

    // __TEXT's link-time address against where the image actually landed. The
    // load commands give link-time addresses, so everything read out of them
    // needs this added to reach the corpse.
    uint64_t slide = imageAddress - Address(*textVMAddress);
    Address trieAddress = Address(*linkeditVMAddress) + slide + trieSegmentOffset;

    if (!hasReadBudget(exportSize))
        return { };
    auto trieBuffer = snapshot.copyBytes(trieAddress, exportSize);
    if (!trieBuffer) {
        Diagnostics::count("unreadable exports tries"_s);
        return { };
    }
    Diagnostics::count("exports tries searched"_s);

    auto found = ExportsTrie::lookUp(trieBuffer->span(), name);
    if (!found) {
        if (found.error() == ExportsTrie::Failure::ReExport)
            Diagnostics::count("re-exports, which are not followed"_s);
        else if (found.error() == ExportsTrie::Failure::UnsupportedKind)
            Diagnostics::count("exports without a single address"_s);
        return { };
    }
    if (found->kind == ExportsTrie::Export::Kind::Absolute)
        return Address(found->value);
    return imageAddress + found->value;
}

Address Symbol::lookUpName(Snapshot& snapshot)
{
    if (!snapshot.isValid()) {
        CORPSE_REPORT("Cannot look up '%s' in an invalid snapshot", m_name.c_str());
        return { };
    }
    int pid = static_cast<int>(snapshot.process()->pid());
    CORPSE_DIAGNOSTICS("looking up the symbol '_%s' in pid %d", m_name.c_str(), pid);

    m_readBudget = maxTotalBytesRead;

    std::string name = "_" + m_name; // Use Mach-O symbol name for look up.

    const Vector<Image>& images = snapshot.images();
    Diagnostics::count("images listed"_s, images.size());
    for (const Image& image : images) {
        if (Address address = resolveInImage(snapshot, image.loadAddress(), name))
            return address;
    }

    if (!Diagnostics::total("exports tries searched"_s))
        CORPSE_REPORT("No symbol '_%s' in pid %d: no exports trie could be read, so this is a memory access problem", m_name.c_str(), pid);
    else
        CORPSE_REPORT("No symbol '_%s' in pid %d: only exported symbols are in an exports trie, so a symbol the linker hid is invisible here", m_name.c_str(), pid);
    return { };
}

#else

Address Symbol::lookUpName(Snapshot&)
{
    CORPSE_REPORT("Symbol lookup is not supported on this platform");
    return { };
}

#endif // OS(DARWIN)

Symbol::Symbol(Snapshot& snapshot, const char* name)
    : m_name(name ? name : "")
{
    if (m_name.empty())
        CORPSE_REPORT("A symbol lookup needs a name");
    else
        m_address = lookUpName(snapshot);
}

} // namespace Corpse
} // namespace JSC

#endif // ENABLE(MYA)
