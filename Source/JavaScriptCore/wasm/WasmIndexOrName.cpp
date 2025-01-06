/*
 * Copyright (C) 2017-2024 Apple Inc. All rights reserved.
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

#include <wtf/PrintStream.h>
#include <wtf/text/MakeString.h>

namespace JSC { namespace Wasm {

IndexOrName::IndexOrName(Index index, std::pair<const Name*, RefPtr<NameSection>>&& name)
{
#if USE(JSVALUE64)
    static_assert(sizeof(m_indexName.index) == sizeof(m_indexName.name), "bit-tagging depends on sizes being equal");
    ASSERT(!(index & allTags));
    ASSERT(!(std::bit_cast<Index>(name.first) & allTags));
    if (name.first)
        m_indexName.name = name.first;
    else
        m_indexName.index = indexTag | index;
#elif USE(JSVALUE32_64)
    if (name.first) {
        m_indexName.name = name.first;
        m_kind = Kind::Name;
    } else {
        m_indexName.index = index;
        m_kind = Kind::Index;
    }
#endif
    m_nameSection = WTFMove(name.second);
}

void IndexOrName::dump(PrintStream& out) const
{
    if (isEmpty() || !nameSection()) {
        out.print("wasm-stub"_s);
        if (isIndex())
            out.print('[', index(), ']');
        return;
    }

    auto moduleName = nameSection()->moduleName.size() ? nameSection()->moduleName.span() : nameSection()->moduleHash.span();
    if (isIndex())
        out.print(moduleName, ".wasm-function["_s, index(), "]");
    else
        out.print(moduleName, ".wasm-function["_s, name()->span(), "]");
}

String makeString(const IndexOrName& ion)
{
    if (ion.isEmpty())
        return "wasm-stub"_s;
    auto moduleName = ion.nameSection()->moduleName.size() ? ion.nameSection()->moduleName.span() : ion.nameSection()->moduleHash.span();
    if (ion.isIndex())
        return makeString(moduleName, ".wasm-function["_s, ion.index(), ']');
    return makeString(moduleName, ".wasm-function["_s, ion.name()->span(), ']');
}

} } // namespace JSC::Wasm
