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

#include "WasmName.h"
#include <wtf/StdLibExtras.h>
#include <wtf/text/WTFString.h>

namespace JSC { namespace Wasm {

struct IndexOrName {
    typedef size_t Index;

    IndexOrName()
        : m_index(emptyTag)
    { }
    IndexOrName(Index, const Name*);
    bool isEmpty() const { return bitwise_cast<Index>(*this) & emptyTag; }
    bool isIndex() const { return bitwise_cast<Index>(*this) & indexTag; }
    bool isName() const { return !(isEmpty() || isName()); }

    friend String makeString(const IndexOrName&);

private:
    union {
        Index m_index;
        const Name* m_name;
    };

    // Use the top bits as tags. Neither pointers nor the function index space should use them.
    static constexpr Index indexTag = 1ull << (CHAR_BIT * sizeof(Index) - 1);
    static constexpr Index emptyTag = 1ull << (CHAR_BIT * sizeof(Index) - 2);
    static constexpr Index allTags = indexTag | emptyTag;
};

String makeString(const IndexOrName&);

} } // namespace JSC::Wasm
