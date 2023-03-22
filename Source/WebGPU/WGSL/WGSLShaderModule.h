/*
 * Copyright (c) 2021 Apple Inc. All rights reserved.
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

#include "ASTDirective.h"
#include "ASTFunction.h"
#include "ASTStructure.h"
#include "ASTVariable.h"
#include "TypeStore.h"
#include "WGSL.h"

#include <wtf/text/StringHash.h>
#include <wtf/text/WTFString.h>

namespace WGSL {

class ShaderModule {
    WTF_MAKE_FAST_ALLOCATED;
public:
    explicit ShaderModule(const String& source)
        : ShaderModule(source, { })
    { }
    
    ShaderModule(const String& source, const Configuration& configuration)
        : m_source(source)
        , m_configuration(configuration)
    { }

    const String& source() const { return m_source; }
    const Configuration& configuration() const { return m_configuration; }
    AST::Directive::List& directives() { return m_directives; }
    AST::Function::List& functions() { return m_functions; }
    AST::Structure::List& structures() { return m_structures; }
    AST::Variable::List& variables() { return m_variables; }
    TypeStore& types() { return m_types; }

    template<typename T>
    std::enable_if_t<std::is_base_of_v<AST::Node, T>, void> replace(T* current, T&& replacement)
    {
        RELEASE_ASSERT(current->kind() == replacement.kind());
        std::swap(*current, replacement);
        m_replacements.append([current, replacement = WTFMove(replacement)]() mutable {
            std::exchange(*current, WTFMove(replacement));
        });
    }

    template<typename T>
    std::enable_if_t<std::is_fundamental_v<T>, void> replace(T* current, T&& replacement)
    {
        std::swap(*current, replacement);
        m_replacements.append([current, replacement = WTFMove(replacement)]() mutable {
            std::exchange(*current, WTFMove(replacement));
        });
    }

    template<typename CurrentType, typename ReplacementType>
    void replace(CurrentType* current, ReplacementType&& replacement)
    {
        static_assert(sizeof(ReplacementType) <= sizeof(CurrentType));

        m_replacements.append([current, currentCopy = *current]() mutable {
            bitwise_cast<ReplacementType*>(current)->~ReplacementType();
            new (current) CurrentType(WTFMove(currentCopy));
        });

        current->~CurrentType();
        new (current) ReplacementType(WTFMove(replacement));
    }

    template<typename T, size_t size>
    T takeLast(const Vector<T, size>& constVector)
    {
        auto& vector = const_cast<Vector<T, size>&>(constVector);
        auto last = vector.takeLast();
        m_replacements.append([&vector, last]() mutable {
            vector.append(WTFMove(last));
        });
        return last;
    }

    template<typename T, size_t size>
    void append(const Vector<T, size>& constVector, T&& value)
    {
        auto& vector = const_cast<Vector<T, size>&>(constVector);
        vector.append(std::forward<T>(value));
        m_replacements.append([&vector]() {
            vector.takeLast();
        });
    }

    template<typename T, size_t size>
    void insert(const Vector<T, size>& constVector, size_t position, T&& value)
    {
        auto& vector = const_cast<Vector<T, size>&>(constVector);
        vector.insert(position, std::forward<T>(value));
        m_replacements.append([&vector, position]() {
            vector.remove(position);
        });
    }

    void revertReplacements()
    {
        for (int i = m_replacements.size() - 1; i >= 0; --i)
            m_replacements[i]();
        m_replacements.clear();
    }

private:
    String m_source;
    Configuration m_configuration;
    AST::Directive::List m_directives;
    AST::Function::List m_functions;
    AST::Structure::List m_structures;
    AST::Variable::List m_variables;
    TypeStore m_types;
    Vector<std::function<void()>> m_replacements;
};

} // namespace WGSL
