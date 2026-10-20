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

#include "config.h"
#include "CorpseError.h"

#if ENABLE(MYA)

#include "CorpseClient.h"

#include <stdarg.h>
#include <stdio.h>
#include <wtf/StdLibExtras.h>
#include <wtf/StringPrintStream.h>

WTF_ALLOW_UNSAFE_BUFFER_USAGE_BEGIN

namespace JSC {
namespace Corpse {

thread_local unsigned Error::s_reportCount = 0;
thread_local Diagnostics* Diagnostics::s_current = nullptr;

void Error::report(const char* format, ...)
{
    ++s_reportCount;

    StringPrintStream out;
    out.print(Client::name(), ": ");
    va_list args;
    va_start(args, format);
    out.vprintf(format, args);
    va_end(args);
    out.print("\n", Diagnostics::context());

    SAFE_FPRINTF(stderr, "%s", out.toUTF8CString());
}

UTF8CString Diagnostics::describe(const char* format, ...)
{
    StringPrintStream out;
    va_list args;
    va_start(args, format);
    out.vprintf(format, args);
    va_end(args);
    return out.toUTF8CString();
}

Diagnostics::Diagnostics(UTF8CString&& operation)
    : m_operation(WTF::move(operation))
    , m_parent(s_current)
{
    s_current = this;
}

Diagnostics::~Diagnostics()
{
    ASSERT(s_current == this);
    s_current = m_parent;
}

static ASCIILiteral label(DiagnosticCounter counter)
{
    switch (counter) {
    case DiagnosticCounter::ImagesListed:
        return "images listed"_s;
    case DiagnosticCounter::ImageHeadersRead:
        return "image headers read"_s;
    case DiagnosticCounter::ImagesInSharedCache:
        return "images in the shared cache"_s;
    case DiagnosticCounter::ExportsTriesSearched:
        return "exports tries searched"_s;
    case DiagnosticCounter::UnreadableImageHeaders:
        return "unreadable image headers"_s;
    case DiagnosticCounter::ImplausibleLoadCommandSizes:
        return "implausible load command sizes"_s;
    case DiagnosticCounter::UnreadableLoadCommands:
        return "unreadable load commands"_s;
    case DiagnosticCounter::ImagesWithoutExportsTrie:
        return "images without an exports trie"_s;
    case DiagnosticCounter::ImplausibleExportsTrieSizes:
        return "implausible exports trie sizes"_s;
    case DiagnosticCounter::ExportsTriesOutsideLinkedit:
        return "exports tries outside __LINKEDIT"_s;
    case DiagnosticCounter::UnreadableExportsTries:
        return "unreadable exports tries"_s;
    case DiagnosticCounter::ImagesSkippedForReadBudget:
        return "images skipped for the read budget"_s;
    case DiagnosticCounter::ReExports:
        return "re-exports, which are not followed"_s;
    case DiagnosticCounter::ExportsWithoutSingleAddress:
        return "exports without a single address"_s;
    case DiagnosticCounter::ThreadsListed:
        return "threads listed"_s;
    case DiagnosticCounter::ThreadStatesRead:
        return "thread states read"_s;
    case DiagnosticCounter::UnreadableThreadStates:
        return "threads with unreadable state"_s;
    }
    RELEASE_ASSERT_NOT_REACHED();
}

UTF8CString Diagnostics::context()
{
    StringPrintStream out;
    for (const Diagnostics* scope = s_current; scope; scope = scope->m_parent)
        scope->print(out);
    return out.toUTF8CString();
}

void Diagnostics::print(PrintStream& out) const
{
    out.print(Client::name(), ":   while ", m_operation);
    ASCIILiteral separator = ": "_s;
    for (size_t i = 0; i < numberOfDiagnosticCounters; ++i) {
        if (!m_counts[i])
            continue;
        out.print(separator, label(static_cast<DiagnosticCounter>(i)), " ", m_counts[i]);
        separator = ", "_s;
    }
    out.print("\n");
}

} // namespace Corpse
} // namespace JSC

WTF_ALLOW_UNSAFE_BUFFER_USAGE_END

#endif // ENABLE(MYA)
