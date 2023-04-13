/*
 * Copyright (C) 2023 Apple Inc. All rights reserved.
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

#include "ASTNode.h"

#include <wtf/PrintStream.h>
#include <wtf/text/WTFString.h>

namespace WGSL::AST {

class Identifier : public Node {
    WTF_MAKE_FAST_ALLOCATED;
public:
    static Identifier make(String& id) { return { SourceSpan::empty(), String(id) }; }
    static Identifier make(String&& id) { return { SourceSpan::empty(), WTFMove(id) }; }
    static Identifier makeWithSpan(SourceSpan span, String&& id) { return { WTFMove(span), WTFMove(id) }; }
    static Identifier makeWithSpan(SourceSpan span, StringView id) { return { WTFMove(span), id }; }

    NodeKind kind() const override;
    operator String&() { return m_id; }
    operator const String&() const { return m_id; }

    const String& id() const { return m_id; }

    void dump(PrintStream&) const;

private:
    Identifier(SourceSpan span, String&& id)
        : Node(span)
        , m_id(WTFMove(id))
    { }

    Identifier(SourceSpan span, StringView id)
        : Identifier(span, id.toString())
    { }

    String m_id;
};

inline void Identifier::dump(PrintStream& out) const
{
    out.print(m_id);
}

} // namespace WGSL::AST

SPECIALIZE_TYPE_TRAITS_WGSL_AST(Identifier);

namespace WTF {

template<> class StringTypeAdapter<WGSL::AST::Identifier, void> : public StringTypeAdapter<StringImpl*, void> {
public:
    StringTypeAdapter(const WGSL::AST::Identifier& id)
        : StringTypeAdapter<StringImpl*, void> { id.id().impl() }
    { }
};

} // namespace WTF
