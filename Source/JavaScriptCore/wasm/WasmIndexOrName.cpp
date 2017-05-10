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

#include "config.h"
#include "WasmIndexOrName.h"

namespace JSC { namespace Wasm {

IndexOrName::IndexOrName(Index index, const Name* name)
{
    static_assert(sizeof(m_index) == sizeof(m_name), "bit-tagging depends on sizes being equal");
    static_assert(sizeof(m_index) == sizeof(*this), "bit-tagging depends on object being the size of the union's types");

    if ((index & allTags) || (bitwise_cast<Index>(name) & allTags))
        *this = IndexOrName();
    else if (name)
        m_name = name;
    else
        m_index = indexTag | index;
}

String makeString(const IndexOrName& ion)
{
    if (ion.isEmpty())
        return String();
    if (ion.isIndex())
        return String::number(ion.m_index & ~IndexOrName::indexTag);
    return String(ion.m_name->data(), ion.m_name->size());
};

} } // namespace JSC::Wasm
