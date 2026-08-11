/*
 * Copyright (C) 2025 Apple Inc. All rights reserved.
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

#if ENABLE(B3_JIT)

#include "B3Value.h"

WTF_ALLOW_UNSAFE_BUFFER_USAGE_BEGIN

namespace JSC::B3 {

class JS_EXPORT_PRIVATE BulkMemoryValue final : public Value {
public:
    static bool accepts(Kind kind) { return kind == MemoryCopy || kind == MemoryFill; }

    ~BulkMemoryValue() final;

    const HeapRange& readRange() const { return m_readRange; }
    void setReadRange(const HeapRange& range) { m_readRange = range; }

    const HeapRange& writeRange() const { return m_writeRange; }
    void setWriteRange(const HeapRange& range) { m_writeRange = range; }

    B3_SPECIALIZE_VALUE_FOR_FIXED_CHILDREN(3)
    B3_SPECIALIZE_VALUE_FOR_FINAL_SIZE_FIXED_CHILDREN

private:
    void dumpMeta(CommaPrinter&, PrintStream&) const final;

    friend class Procedure;
    friend class Value;

    BulkMemoryValue(Kind, Origin, Value*, Value*, Value*);

    HeapRange m_readRange { HeapRange::top() };
    HeapRange m_writeRange { HeapRange::top() };
};

} // namespace JSC::B3

WTF_ALLOW_UNSAFE_BUFFER_USAGE_END

#endif // ENABLE(B3_JIT)
