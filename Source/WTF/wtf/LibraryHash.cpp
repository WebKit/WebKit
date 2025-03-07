/*
 * Copyright (C) 2024 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include <wtf/LibraryHash.h>

#include <wtf/DataLog.h>
#include <wtf/HexNumber.h>
#include <wtf/NeverDestroyed.h>
#include <wtf/text/SuperFastHash.h>

#if OS(DARWIN)
#include <dlfcn.h>
#include <mach-o/dyld.h>
#include <uuid/uuid.h>
#include <wtf/spi/darwin/dyldSPI.h>
#elif OS(WINDOWS)
#include <windows.h>
#elif OS(UNIX)
#include <dlfcn.h>
#include <link.h>

/* Define ElfW for compatibility with Linux, prefer __ElfN() in FreeBSD code */
#if !defined(ElfW)
#define ElfW(x) __ElfN(x)
#endif

#if !defined(NT_GNU_BUILD_ID)
#define NT_GNU_BUILD_ID 3
#endif

#endif

namespace WTF {

std::optional<unsigned> LibraryHash::compute(void* function)
{
    static constexpr bool verbose = false;
    UNUSED_VARIABLE(verbose);

#if OS(DARWIN)
    uuid_t uuid;
    if (const mach_header* header = dyld_image_header_containing_address(function); header && _dyld_get_image_uuid(header, uuid)) {
        uuid_string_t uuidString = { };
        uuid_unparse(uuid, uuidString);
        unsigned hash = SuperFastHash::computeHash(uuidString);
        dataLogLnIf(verbose, "UUID of JavaScriptCore.framework:", uuidString);
        return hash;
    }
    return std::nullopt;
#elif OS(WINDOWS)
    // https://deplinenoise.wordpress.com/2013/06/14/getting-your-pdb-name-from-a-running-executable-windows/
    struct PdbInfo {
        uint32_t signature;
        uint8_t guid[16];
        uint32_t age;
    };

    HMODULE hModule = nullptr;
    if (BOOL result = GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, reinterpret_cast<char*>(function), &hModule); !result)
        return std::nullopt;

    uintptr_t baseAddress = std::bit_cast<uintptr_t>(hModule);

    // This is where the MZ...blah header lives (the DOS header)
    auto* dos = std::bit_cast<IMAGE_DOS_HEADER*>(baseAddress);

    // We want the PE header.
    auto* file = std::bit_cast<IMAGE_FILE_HEADER*>(baseAddress + dos->e_lfanew + 4);

    if (file->SizeOfOptionalHeader == 0)
        return std::nullopt;

    // Straight after that is the optional header (which technically is optional, but in practice always there.)
    auto* opt = std::bit_cast<IMAGE_OPTIONAL_HEADER*>(std::bit_cast<uintptr_t>(file) + sizeof(IMAGE_FILE_HEADER));

    if (opt->NumberOfRvaAndSizes <= IMAGE_DIRECTORY_ENTRY_DEBUG)
        return std::nullopt;

    // Grab the debug data directory which has an indirection to its data
    auto* dir = &opt->DataDirectory[IMAGE_DIRECTORY_ENTRY_DEBUG];

    if (dir->Size == 0)
        return std::nullopt;

    // Convert that data to the right type.
    auto* debugDirectory = std::bit_cast<IMAGE_DEBUG_DIRECTORY*>(baseAddress + dir->VirtualAddress);

    if (debugDirectory->Type != IMAGE_DEBUG_TYPE_CODEVIEW)
        return std::nullopt;

    auto* info = std::bit_cast<PdbInfo*>(baseAddress + debugDirectory->AddressOfRawData);

    // 'R' 'S' 'D' 'S'
    if (info->signature != 0x53445352)
        return std::nullopt;

    return SuperFastHash::computeHash(std::span<const uint8_t> { info->guid, 16U });
#elif OS(UNIX)
    Dl_info info { };
    if (!dladdr(function, &info))
        return std::nullopt;

    if (!info.dli_fbase)
        return std::nullopt;

    struct DLParam {
        void* start { nullptr };
        std::span<const uint8_t> description;
    };

    DLParam param { };
    param.start = info.dli_fbase;
    if (!dl_iterate_phdr(static_cast<int(*)(struct dl_phdr_info*, size_t, void*)>(
        [](struct dl_phdr_info* info, size_t, void* priv) -> int {
            WTF_ALLOW_UNSAFE_BUFFER_USAGE_BEGIN // Unix port
            auto* data = static_cast<DLParam*>(priv);
            void* start = nullptr;
            for (unsigned i = 0; i < info->dlpi_phnum; ++i) {
                if (info->dlpi_phdr[i].p_type == PT_LOAD) {
                    start = std::bit_cast<void*>(static_cast<uintptr_t>(info->dlpi_addr + info->dlpi_phdr[i].p_vaddr));
                    break;
                }
            }

            if (start != data->start)
                return 0;

            for (unsigned i = 0; i < info->dlpi_phnum; ++i) {
                if (info->dlpi_phdr[i].p_type != PT_NOTE)
                    continue;

                // https://refspecs.linuxbase.org/elf/gabi4+/ch5.pheader.html#note_section
                using NoteHeader = ElfW(Nhdr);

                auto* payload = std::bit_cast<uint8_t*>(static_cast<uintptr_t>(info->dlpi_addr + info->dlpi_phdr[i].p_vaddr));
                size_t length = info->dlpi_phdr[i].p_filesz;
                for (size_t index = 0; index < length;) {
                    auto* cursor  = payload + index;
                    if ((index + sizeof(NoteHeader)) > length)
                        return 0;

                    auto* note = std::bit_cast<NoteHeader*>(cursor);
                    size_t size = sizeof(NoteHeader) + roundUpToMultipleOf<4>(note->n_namesz) + roundUpToMultipleOf<4>(note->n_descsz);
                    if ((index + size) > length)
                        return 0;

                    auto* name = cursor + sizeof(NoteHeader);
                    auto* description = cursor + sizeof(NoteHeader) + roundUpToMultipleOf<4>(note->n_namesz);

                    if (note->n_type == NT_GNU_BUILD_ID && note->n_descsz != 0 && note->n_namesz == 4 && memcmp(name, "GNU", 4) == 0) {
                        // Found build-id note.
                        data->description = std::span { description, note->n_descsz };
                        return 1;
                    }

                    index += size;
                }
            }
            return 0;
            WTF_ALLOW_UNSAFE_BUFFER_USAGE_END
        }), &param))
        return std::nullopt;

    if (param.description.empty()) {
        WTFReportBacktrace();
        WTFCrash();
        return std::nullopt;
    }

    if constexpr (verbose) {
        for (uint8_t value : param.description)
            dataLog(hex(value));
        dataLogLn("");
    }

    return SuperFastHash::computeHash(param.description);
#else
    return std::nullopt;
#endif
}

} // namespace WTF
