/*
 * Copyright (C) 2022-2024 Apple Inc. All rights reserved.
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
#include "JSCBytecodeCacheVersion.h"

#include <wtf/DataLog.h>
#include <wtf/HexNumber.h>
#include <wtf/NeverDestroyed.h>
#include <wtf/text/SuperFastHash.h>

#if OS(UNIX)
#include <dlfcn.h>
#if OS(DARWIN)
#include <mach-o/dyld.h>
#include <uuid/uuid.h>
#include <wtf/spi/darwin/dyldSPI.h>
#else
#include <link.h>
#endif
#endif

namespace JSC {

namespace JSCBytecodeCacheVersionInternal {
static constexpr bool verbose = false;
}

static uint32_t byteCodeCacheVersion = 0;

void dangerouslyOverrideJSCBytecodeCacheVersion(uint32_t version)
{
    byteCodeCacheVersion = version;
}

uint32_t computeJSCBytecodeCacheVersion()
{
#if USE(BUN_JSC_ADDITIONS)
    if (byteCodeCacheVersion != 0) {
        return byteCodeCacheVersion;
    }

    static constexpr uint32_t precomputedCacheVersion = SuperFastHash::computeHash(__TIMESTAMP__);
    byteCodeCacheVersion = precomputedCacheVersion;
    return precomputedCacheVersion;
#else
    UNUSED_VARIABLE(JSCBytecodeCacheVersionInternal::verbose);
    static LazyNeverDestroyed<uint32_t> cacheVersion;
    static std::once_flag onceFlag;
    std::call_once(onceFlag, [] {
        void* jsFunctionAddr = bitwise_cast<void*>(&computeJSCBytecodeCacheVersion);
#if OS(DARWIN)
        uuid_t uuid;
        if (const mach_header* header = dyld_image_header_containing_address(jsFunctionAddr); header && _dyld_get_image_uuid(header, uuid)) {
            uuid_string_t uuidString = { };
            uuid_unparse(uuid, uuidString);
            cacheVersion.construct(SuperFastHash::computeHash(uuidString));
            dataLogLnIf(JSCBytecodeCacheVersionInternal::verbose, "UUID of JavaScriptCore.framework:", uuidString);
            return;
        }
        cacheVersion.construct(0);
        dataLogLnIf(JSCBytecodeCacheVersionInternal::verbose, "Failed to get UUID for JavaScriptCore framework");
#elif OS(UNIX) && !PLATFORM(PLAYSTATION)
        auto result = ([&] -> std::optional<uint32_t> {
            Dl_info info { };
            if (!dladdr(jsFunctionAddr, &info))
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
                    auto* data = static_cast<DLParam*>(priv);
                    void* start = nullptr;
                    for (unsigned i = 0; i < info->dlpi_phnum; ++i) {
                        if (info->dlpi_phdr[i].p_type == PT_LOAD) {
                            start = bitwise_cast<void*>(static_cast<uintptr_t>(info->dlpi_addr + info->dlpi_phdr[i].p_vaddr));
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

                        auto* payload = bitwise_cast<uint8_t*>(static_cast<uintptr_t>(info->dlpi_addr + info->dlpi_phdr[i].p_vaddr));
                        size_t length = info->dlpi_phdr[i].p_filesz;
                        for (size_t index = 0; index < length;) {
                            auto* cursor  = payload + index;
                            if ((index + sizeof(NoteHeader)) > length)
                                return 0;

                            auto* note = bitwise_cast<NoteHeader*>(cursor);
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
                }), &param))
                    return std::nullopt;

                if (param.description.empty())
                    return std::nullopt;

                if constexpr (JSCBytecodeCacheVersionInternal::verbose) {
                    for (uint8_t value : param.description)
                        dataLog(hex(value));
                    dataLogLn("");
                }

                return SuperFastHash::computeHash(param.description);
        }());
        if (result) {
            cacheVersion.construct(result.value());
            return;
        }
        cacheVersion.construct(0);
        dataLogLnIf(JSCBytecodeCacheVersionInternal::verbose, "Failed to get UUID for JavaScriptCore framework");
#else
        UNUSED_VARIABLE(jsFunctionAddr);
        static constexpr uint32_t precomputedCacheVersion = SuperFastHash::computeHash(__TIMESTAMP__);
        cacheVersion.construct(precomputedCacheVersion);
#endif
    });
    return cacheVersion.get();
#endif
}

} // namespace JSC
