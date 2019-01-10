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

#include "WHLSLEnumerationMember.h"
#include "WHLSLLexer.h"
#include "WHLSLNamedType.h"
#include "WHLSLUnnamedType.h"
#include <memory>
#include <wtf/HashMap.h>
#include <wtf/UniqueRef.h>
#include <wtf/Vector.h>
#include <wtf/text/StringHash.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

namespace WHLSL {

namespace AST {

class EnumerationDefinition : public NamedType {
public:
    EnumerationDefinition(Lexer::Token&& origin, String&& name, Optional<UniqueRef<UnnamedType>>&& type)
        : NamedType(WTFMove(origin), WTFMove(name))
        , m_type(WTFMove(type))
    {
    }

    virtual ~EnumerationDefinition() = default;

    EnumerationDefinition(const EnumerationDefinition&) = delete;
    EnumerationDefinition(EnumerationDefinition&&) = default;

    bool isEnumerationDefinition() const override { return true; }

    UnnamedType* type() { return m_type ? &static_cast<UnnamedType&>(*m_type) : nullptr; }

    bool add(EnumerationMember&& member)
    {
        auto result = m_members.add(member.name(), std::make_unique<EnumerationMember>(WTFMove(member)));
        return !result.isNewEntry;
    }

    EnumerationMember* memberByName(const String& name)
    {
        auto iterator = m_members.find(name);
        if (iterator == m_members.end())
            return nullptr;
        return iterator->value.get();
    }

    Vector<std::reference_wrapper<EnumerationMember>> enumerationMembers()
    {
        Vector<std::reference_wrapper<EnumerationMember>> result;
        for (auto& pair : m_members)
            result.append(*pair.value);
        return result;
    }

private:
    Optional<UniqueRef<UnnamedType>> m_type;
    HashMap<String, std::unique_ptr<EnumerationMember>> m_members;
};

} // namespace AST

}

}

SPECIALIZE_TYPE_TRAITS_WHLSL_NAMED_TYPE(EnumerationDefinition, isEnumerationDefinition())

#endif
