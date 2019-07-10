/*
 * Copyright (C) 2019 Apple Inc. All rights reserved.
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

#if ENABLE(WEBGPU)

#include "WHLSLExpression.h"
#include "WHLSLLexer.h"
#include "WHLSLQualifier.h"
#include "WHLSLSemantic.h"
#include "WHLSLType.h"
#include <memory>
#include <wtf/UniqueRef.h>
#include <wtf/Vector.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

namespace WHLSL {

namespace AST {

class VariableDeclaration final {
// Final because we made the destructor non-virtual.
public:
    VariableDeclaration(CodeLocation codeLocation, Qualifiers&& qualifiers, Optional<UniqueRef<UnnamedType>>&& type, String&& name, std::unique_ptr<Semantic>&& semantic, std::unique_ptr<Expression>&& initializer)
        : m_codeLocation(codeLocation)
        , m_qualifiers(WTFMove(qualifiers))
        , m_type(WTFMove(type))
        , m_name(WTFMove(name))
        , m_semantic(WTFMove(semantic))
        , m_initializer(WTFMove(initializer))
    {
    }

    ~VariableDeclaration() = default;

    VariableDeclaration(const VariableDeclaration&) = delete;
    VariableDeclaration(VariableDeclaration&&) = default;

    String& name() { return m_name; }

    // We use this for ReadModifyWrite expressions, since we don't know the type of their
    // internal variables until the checker runs. All other variables should start life out
    // with a type.
    void setType(UniqueRef<UnnamedType> type)
    {
        ASSERT(!m_type);
        m_type = WTFMove(type);
    }
    const Optional<UniqueRef<UnnamedType>>& type() const { return m_type; }
    UnnamedType* type() { return m_type ? &*m_type : nullptr; }
    Semantic* semantic() { return m_semantic.get(); }
    Expression* initializer() { return m_initializer.get(); }
    bool isAnonymous() const { return m_name.isNull(); }
    std::unique_ptr<Expression> takeInitializer() { return WTFMove(m_initializer); }
    void setInitializer(std::unique_ptr<Expression> expression)
    {
        ASSERT(!initializer());
        ASSERT(expression);
        m_initializer = WTFMove(expression);
    }
    CodeLocation codeLocation() const { return m_codeLocation; }

private:
    CodeLocation m_codeLocation;
    Qualifiers m_qualifiers;
    Optional<UniqueRef<UnnamedType>> m_type;
    String m_name;
    std::unique_ptr<Semantic> m_semantic;
    std::unique_ptr<Expression> m_initializer;
};

using VariableDeclarations = Vector<UniqueRef<VariableDeclaration>>;

} // namespace AST

}

}

#endif
