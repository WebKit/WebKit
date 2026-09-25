/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
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

#include <stdint.h>
#include <utility>
#include <wtf/Assertions.h>
#include <wtf/Noncopyable.h>
#include <wtf/Vector.h>
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

// Mya may be used on corrupted target heaps; we must be able to
// use and test it in these cases without crashing, and
// let the operator see what is going on.
//
// A scope names the operation in progress on this thread. Counters go to the
// innermost scope, and every scope prints when an error is reported.
class Diagnostics {
    WTF_MAKE_NONCOPYABLE(Diagnostics);
public:
    Diagnostics(const char* format, ...) WTF_ATTRIBUTE_PRINTF(2, 3);
    ~Diagnostics();

    static void count(ASCIILiteral what, uint64_t by = 1);
    static uint64_t total(ASCIILiteral what);

private:
    void print() const;

    CString m_operation;
    Vector<std::pair<ASCIILiteral, uint64_t>> m_counters;
    Diagnostics* const m_parent;

    static thread_local Diagnostics* s_current;

    friend class Error;
};

} // namespace Corpse
} // namespace JSC

#define CORPSE_REPORT(format, ...) \
    WTF_ALLOW_UNSAFE_BUFFER_USAGE_BEGIN \
    ::JSC::Corpse::Error::report(format __VA_OPT__(, LOG_PRINTF_TYPE(__VA_ARGS__))) \
    WTF_ALLOW_UNSAFE_BUFFER_USAGE_END

#define CORPSE_DIAGNOSTICS(format, ...) \
    WTF_ALLOW_UNSAFE_BUFFER_USAGE_BEGIN \
    ::JSC::Corpse::Diagnostics diagnosticsScope(format __VA_OPT__(, LOG_PRINTF_TYPE(__VA_ARGS__))) \
    WTF_ALLOW_UNSAFE_BUFFER_USAGE_END

#endif // ENABLE(MYA)
