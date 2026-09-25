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

#pragma once

#include <JavaScriptCore/CorpsePlatform.h>

#if ENABLE(MYA)

#include <array>
#include <stdint.h>
#include <wtf/Assertions.h>
#include <wtf/Noncopyable.h>
#include <wtf/Nonmovable.h>
#include <wtf/PrintStream.h>
#include <wtf/text/ASCIILiteral.h>
#include <wtf/text/CString.h>

namespace JSC {
namespace Corpse {

// Reports the library's diagnostics. Messages are prefixed with the name the
// client set via Corpse::Client, so they read as the client's own output.
class Error {
public:
    static void report(const char* format, ...) WTF_ATTRIBUTE_PRINTF(1, 2);

    static unsigned reportCount() { return s_reportCount; }

private:
    static thread_local unsigned s_reportCount;
};

enum class DiagnosticCounter : uint8_t {
    ImagesListed,
    ImageHeadersRead,
    ImagesInSharedCache,
    ExportsTriesSearched,
    UnreadableImageHeaders,
    ImplausibleLoadCommandSizes,
    UnreadableLoadCommands,
    ImagesWithoutExportsTrie,
    ImplausibleExportsTrieSizes,
    ExportsTriesOutsideLinkedit,
    UnreadableExportsTries,
    ImagesSkippedForReadBudget,
    ReExports,
    ExportsWithoutSingleAddress,
    ThreadsListed,
    ThreadStatesRead,
    UnreadableThreadStates,
};
constexpr size_t numberOfDiagnosticCounters = static_cast<size_t>(DiagnosticCounter::UnreadableThreadStates) + 1;

// Mya may be used on corrupted target heaps; we must be able to
// use and test it in these cases without crashing, and
// let the operator see what is going on.
class Diagnostics {
    WTF_MAKE_NONCOPYABLE(Diagnostics);
    WTF_MAKE_NONMOVABLE(Diagnostics);
public:
    explicit Diagnostics(UTF8CString&& operation);
    ~Diagnostics();

    static UTF8CString describe(const char* format, ...) WTF_ATTRIBUTE_PRINTF(1, 2);

    static void count(DiagnosticCounter counter, uint64_t by = 1)
    {
        if (s_current)
            s_current->m_counts[static_cast<size_t>(counter)] += by;
    }

    uint64_t value(DiagnosticCounter counter) const { return m_counts[static_cast<size_t>(counter)]; }

    static UTF8CString context();

private:
    void print(PrintStream&) const;

    UTF8CString m_operation;
    std::array<uint64_t, numberOfDiagnosticCounters> m_counts { };
    Diagnostics* const m_parent;

    static thread_local Diagnostics* s_current;
};

} // namespace Corpse
} // namespace JSC

#define CORPSE_REPORT(format, ...) \
    WTF_ALLOW_UNSAFE_BUFFER_USAGE_BEGIN \
    ::JSC::Corpse::Error::report(format __VA_OPT__(, LOG_PRINTF_TYPE(__VA_ARGS__))) \
    WTF_ALLOW_UNSAFE_BUFFER_USAGE_END

#define CORPSE_DESCRIBE(format, ...) \
    WTF_ALLOW_UNSAFE_BUFFER_USAGE_BEGIN \
    ::JSC::Corpse::Diagnostics::describe(format __VA_OPT__(, LOG_PRINTF_TYPE(__VA_ARGS__))) \
    WTF_ALLOW_UNSAFE_BUFFER_USAGE_END

#define CORPSE_DIAGNOSTICS(name, format, ...) \
    ::JSC::Corpse::Diagnostics name(CORPSE_DESCRIBE(format __VA_OPT__(, __VA_ARGS__)))

#endif // ENABLE(MYA)
