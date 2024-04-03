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

#if PLATFORM(COCOA)

#include <wtf/ArgumentCoder.h>
#include <wtf/Vector.h>
#include <wtf/text/WTFString.h>

OBJC_CLASS NSPresentationIntent;

namespace WebKit {

class CoreIPCPresentationIntent {
    WTF_MAKE_FAST_ALLOCATED;
public:
    CoreIPCPresentationIntent(NSPresentationIntent *);

    RetainPtr<id> toID() const;

private:
    friend struct IPC::ArgumentCoder<CoreIPCPresentationIntent, void>;

    CoreIPCPresentationIntent(int64_t intentKind, int64_t identity, std::unique_ptr<CoreIPCPresentationIntent>&& parentIntent, int64_t column, Vector<int64_t>&& columnAlignments, int64_t columnCount, int64_t headerLevel, const String& languageHint, int64_t ordinal, int64_t row)
        : m_intentKind(intentKind)
        , m_identity(identity)
        , m_parentIntent(WTFMove(parentIntent))
        , m_column(column)
        , m_columnAlignments(WTFMove(columnAlignments))
        , m_columnCount(columnCount)
        , m_headerLevel(headerLevel)
        , m_languageHint(languageHint)
        , m_ordinal(ordinal)
        , m_row(row)
    {
    }

    int64_t m_intentKind { 0 };
    int64_t m_identity { 0 };
    std::unique_ptr<CoreIPCPresentationIntent> m_parentIntent;

    int64_t m_column { 0 };
    Vector<int64_t> m_columnAlignments;
    int64_t m_columnCount { 0 };
    int64_t m_headerLevel { 0 };
    String m_languageHint;
    int64_t m_ordinal { 0 };
    int64_t m_row { 0 };
};

} // namespace WebKit

#endif // PLATFORM(COCOA)
