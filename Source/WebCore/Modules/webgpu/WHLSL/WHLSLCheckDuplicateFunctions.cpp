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

#include "config.h"
#include "WHLSLCheckDuplicateFunctions.h"

#if ENABLE(WEBGPU)

#include "WHLSLArrayReferenceType.h"
#include "WHLSLArrayType.h"
#include "WHLSLInferTypes.h"
#include "WHLSLTypeReference.h"
#include <wtf/HashSet.h>
#include <wtf/HashTraits.h>

namespace WebCore {

namespace WHLSL {

class DuplicateFunctionKey {
public:
    DuplicateFunctionKey() = default;
    DuplicateFunctionKey(WTF::HashTableDeletedValueType)
    {
        m_function = bitwise_cast<AST::FunctionDeclaration*>(static_cast<uintptr_t>(1));
    }

    DuplicateFunctionKey(const AST::FunctionDeclaration& function)
        : m_function(&function)
    { }

    bool isEmptyValue() const { return !m_function; }
    bool isHashTableDeletedValue() const { return m_function == bitwise_cast<AST::FunctionDeclaration*>(static_cast<uintptr_t>(1)); }

    unsigned hash() const
    {
        unsigned hash = IntHash<size_t>::hash(m_function->parameters().size());
        hash ^= m_function->name().hash();
        for (size_t i = 0; i < m_function->parameters().size(); ++i)
            hash ^= m_function->parameters()[i]->type().value()->hash();

        if (m_function->isCast())
            hash ^= m_function->type().hash();

        return hash;
    }

    bool operator==(const DuplicateFunctionKey& other) const
    {
        if (m_function->parameters().size() != other.m_function->parameters().size())
            return false;

        if (m_function->name() != other.m_function->name())
            return false;

        ASSERT(m_function->isCast() == other.m_function->isCast());

        for (size_t i = 0; i < m_function->parameters().size(); ++i) {
            if (!matches(*m_function->parameters()[i]->type(), *other.m_function->parameters()[i]->type()))
                return false;
        }

        if (!m_function->isCast())
            return true;

        if (matches(m_function->type(), other.m_function->type()))
            return true;

        return false;
    }

    struct Hash {
        static unsigned hash(const DuplicateFunctionKey& key)
        {
            return key.hash();
        }

        static bool equal(const DuplicateFunctionKey& a, const DuplicateFunctionKey& b)
        {
            return a == b;
        }

        static const bool safeToCompareToEmptyOrDeleted = false;
        static const bool emptyValueIsZero = true;
    };

    struct Traits : public WTF::SimpleClassHashTraits<DuplicateFunctionKey> {
        static const bool hasIsEmptyValueFunction = true;
        static bool isEmptyValue(const DuplicateFunctionKey& key) { return key.isEmptyValue(); }
    };

private:
    const AST::FunctionDeclaration* m_function { nullptr };
};

bool checkDuplicateFunctions(const Program& program)
{
    auto passesStaticChecks = [&] (const AST::FunctionDeclaration& function) {
        if (function.name() == "operator&[]" && function.parameters().size() == 2
            && is<AST::ArrayReferenceType>(static_cast<const AST::UnnamedType&>(*function.parameters()[0]->type()))) {
            auto& type = static_cast<const AST::UnnamedType&>(*function.parameters()[1]->type());
            if (is<AST::TypeReference>(type)) {
                // FIXME: https://bugs.webkit.org/show_bug.cgi?id=198161 Shouldn't we already know whether the types have been resolved by now?
                if (auto* resolvedType = downcast<AST::TypeReference>(type).maybeResolvedType()) {
                    if (is<AST::NativeTypeDeclaration>(*resolvedType)) {
                        auto& nativeTypeDeclaration = downcast<AST::NativeTypeDeclaration>(*resolvedType);
                        if (nativeTypeDeclaration.name() == "uint")
                            return false;
                    }
                }
            }
        } else if (function.name() == "operator.length" && function.parameters().size() == 1
            && (is<AST::ArrayReferenceType>(static_cast<const AST::UnnamedType&>(*function.parameters()[0]->type()))
            || is<AST::ArrayType>(static_cast<const AST::UnnamedType&>(*function.parameters()[0]->type()))))
            return false;
        else if (function.name() == "operator=="
            && function.parameters().size() == 2
            && is<AST::ReferenceType>(static_cast<const AST::UnnamedType&>(*function.parameters()[0]->type()))
            && is<AST::ReferenceType>(static_cast<const AST::UnnamedType&>(*function.parameters()[1]->type()))
            && matches(*function.parameters()[0]->type(), *function.parameters()[1]->type()))
            return false;
        else if (function.isCast() && function.parameters().isEmpty()) {
            auto& unifyNode = function.type().unifyNode();
            if (is<AST::NamedType>(unifyNode) && is<AST::NativeTypeDeclaration>(downcast<AST::NamedType>(unifyNode))) {
                auto& nativeTypeDeclaration = downcast<AST::NativeTypeDeclaration>(downcast<AST::NamedType>(unifyNode));
                if (nativeTypeDeclaration.isOpaqueType())
                    return false;
            }
        }

        return true;
    };

    HashSet<DuplicateFunctionKey, DuplicateFunctionKey::Hash, DuplicateFunctionKey::Traits> functions;

    auto add = [&] (const AST::FunctionDeclaration& function) {
        auto addResult = functions.add(DuplicateFunctionKey { function });
        if (!addResult.isNewEntry)
            return false;
        return passesStaticChecks(function);
    };

    for (auto& functionDefinition : program.functionDefinitions()) {
        if (!add(functionDefinition.get()))
            return false;
    }

    for (auto& nativeFunctionDeclaration : program.nativeFunctionDeclarations()) {
        // We generate duplicate native function declarations in synthesize constructors.
        // FIXME: is this right?
        // https://bugs.webkit.org/show_bug.cgi?id=198580
        //
        // Since we do that, we just need to make sure no native function is a duplicate
        // of a user-defined function.

        // FIXME: Add back this assert once we begin to auto generate these in the compiler
        // instead of having them in the stdlib
        // https://bugs.webkit.org/show_bug.cgi?id=198861
        // ASSERT(passesStaticChecks(nativeFunctionDeclaration.get()));
        if (functions.contains(DuplicateFunctionKey { nativeFunctionDeclaration.get() }))
            return false;
    }

    return true;
}

} // namespace WHLSL

} // namespace WebCore

#endif // ENABLE(WEBGPU)
