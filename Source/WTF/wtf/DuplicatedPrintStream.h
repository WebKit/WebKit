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

#pragma once

#include <wtf/PrintStream.h>
#include <wtf/Vector.h>

namespace WTF {

class DuplicatedPrintStream final : public PrintStream {
    WTF_MAKE_FAST_ALLOCATED;
    WTF_MAKE_NONCOPYABLE(DuplicatedPrintStream);
public:
    DuplicatedPrintStream(Vector<std::unique_ptr<PrintStream>>&& streams)
        : m_streams(WTFMove(streams))
    { }

    WTF_EXPORT_PRIVATE ~DuplicatedPrintStream() final;

    template<typename... Stream>
    static inline std::unique_ptr<DuplicatedPrintStream> open(Stream&&... streams)
    {
        return makeUnique<DuplicatedPrintStream>(Vector<std::unique_ptr<PrintStream>>::from(WTFMove(streams)...));
    }

    WTF_EXPORT_PRIVATE void vprintf(const char* format, va_list) final WTF_ATTRIBUTE_PRINTF(2, 0);

private:
    Vector<std::unique_ptr<PrintStream>> m_streams;
};

} // namespace WTF

using WTF::DuplicatedPrintStream;
