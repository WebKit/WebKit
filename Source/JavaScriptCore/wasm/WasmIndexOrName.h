/*
 * Copyright (C) 2017 Apple Inc. All rights reserved.
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

#include <JavaScriptCore/WasmName.h>
#include <JavaScriptCore/WasmNameSection.h>
#include <wtf/RefPtr.h>
#include <wtf/StdLibExtras.h>
#include <wtf/text/WTFString.h>

namespace JSC {

class LLIntOffsetsExtractor;

namespace Wasm {

struct NameSection;

// Keep this class copyable when the world is stopped: do not allocate any memory while copying this.
// SamplingProfiler copies it while suspending threads.
struct IndexOrName {
    friend class JSC::LLIntOffsetsExtractor;
    typedef size_t Index;

public:

    IndexOrName()
    {
        m_indexName.index = emptyTag;
    }

    IndexOrName(Index, std::pair<const Name*, RefPtr<NameSection>>&&);

    bool isEmpty() const
    {
        return std::bit_cast<Index>(m_indexName) & emptyTag;
    }

    bool isIndex() const
    {
        return std::bit_cast<Index>(m_indexName) & indexTag;
    }

    bool isName() const
    {
        return !(isEmpty() || isIndex());
    }

    Index index() const
    {
        ASSERT(isIndex());
        return m_indexName.index & ~indexTag;
    }

    const Name* name() const
    {
        ASSERT(isName());
        return m_indexName.name;
    }

    const std::span<const char8_t> moduleName() const
    {
        if (nameSection()->moduleName.size())
            return nameSection()->moduleName.span();
        if (nameSection()->moduleHash.size())
            return nameSection()->moduleHash.span();
        return std::span<const char8_t>();
    }

    NameSection* nameSection() const { return m_nameSection.get(); }
    void dump(PrintStream&) const;

private:
    union {
        Index index;
        const Name* name;
    } m_indexName;
    RefPtr<NameSection> m_nameSection;

    public:
    // Use the top bits as tags. Neither pointers nor the function index space should use them.
    static constexpr Index indexTag = 1ull << (CHAR_BIT * sizeof(Index) - 1);
    static constexpr Index emptyTag = 1ull << (CHAR_BIT * sizeof(Index) - 2);
    static constexpr Index allTags = indexTag | emptyTag;
    private:
};

String makeString(const IndexOrName&);

} } // namespace JSC::Wasm
