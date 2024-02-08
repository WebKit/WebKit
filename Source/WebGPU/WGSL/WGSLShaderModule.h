/*
 * Copyright (c) 2021-2023 Apple Inc. All rights reserved.
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

#include "ASTBuilder.h"
#include "ASTDeclaration.h"
#include "ASTDirective.h"
#include "ASTIdentityExpression.h"
#include "TypeStore.h"
#include "WGSL.h"
#include "WGSLEnums.h"

#include <wtf/HashSet.h>
#include <wtf/OptionSet.h>
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
    AST::Declaration::List& declarations() { return m_declarations; }
    const AST::Declaration::List& declarations() const { return m_declarations; }
    AST::Directive::List& directives() { return m_directives; }
    TypeStore& types() { return m_types; }
    AST::Builder& astBuilder() { return m_astBuilder; }

    bool usesExternalTextures() const { return m_usesExternalTextures; }
    void setUsesExternalTextures() { m_usesExternalTextures = true; }
    void clearUsesExternalTextures() { m_usesExternalTextures = false; }

    bool usesPackArray() const { return m_usesPackArray; }
    void setUsesPackArray() { m_usesPackArray = true; }
    void clearUsesPackArray() { m_usesPackArray = false; }

    bool usesUnpackArray() const { return m_usesUnpackArray; }
    void setUsesUnpackArray() { m_usesUnpackArray = true; }
    void clearUsesUnpackArray() { m_usesUnpackArray = false; }

    bool usesWorkgroupUniformLoad() const { return m_usesWorkgroupUniformLoad; }
    void setUsesWorkgroupUniformLoad() { m_usesWorkgroupUniformLoad = true; }

    bool usesDivision() const { return m_usesDivision; }
    void setUsesDivision() { m_usesDivision = true; }

    bool usesModulo() const { return m_usesModulo; }
    void setUsesModulo() { m_usesModulo = true; }

    bool usesFrexp() const { return m_usesFrexp; }
    void setUsesFrexp() { m_usesFrexp = true; }

    bool usesModf() const { return m_usesModf; }
    void setUsesModf() { m_usesModf = true; }

    bool usesAtomicCompareExchange() const { return m_usesAtomicCompareExchange; }
    void setUsesAtomicCompareExchange() { m_usesAtomicCompareExchange = true; }
    bool usesFragDepth() const { return m_usesFragDepth; }
    void setUsesFragDepth() { m_usesFragDepth = true; }

    bool usesDot() const { return m_usesDot; }
    void setUsesDot() { m_usesDot = true; }

    bool usesFirstLeadingBit() const { return m_usesFirstLeadingBit; }
    void setUsesFirstLeadingBit() { m_usesFirstLeadingBit = true; }

    bool usesFirstTrailingBit() const { return m_usesFirstTrailingBit; }
    void setUsesFirstTrailingBit() { m_usesFirstTrailingBit = true; }

    bool usesSign() const { return m_usesSign; }
    void setUsesSign() { m_usesSign = true; }

    bool usesSampleMask() const { return m_usesSampleMask; }
    void setUsesSampleMask() { m_usesSampleMask = true; }

    bool usesFrontFacing() const { return m_usesFrontFacing; }
    void setUsesFrontFacing() { m_usesFrontFacing = true; }

    bool usesSampleIndex() const { return m_usesSampleIndex; }
    void setUsesSampleIndex() { m_usesSampleIndex = true; }

    bool usesDot4I8Packed() const { return m_usesDot4I8Packed; }
    void setUsesDot4I8Packed() { m_usesDot4I8Packed = true; }

    bool usesDot4U8Packed() const { return m_usesDot4U8Packed; }
    void setUsesDot4U8Packed() { m_usesDot4U8Packed = true; }

    bool usesExtractBits() const { return m_usesExtractBits; }
    void setUsesExtractBits() { m_usesExtractBits = true; }

    template<typename T>
    std::enable_if_t<std::is_base_of_v<AST::Node, T>, void> replace(T* current, T&& replacement)
    {
        RELEASE_ASSERT(current->kind() == replacement.kind());
        std::swap(*current, replacement);
        m_replacements.append([current, replacement = std::forward<T>(replacement)]() mutable {
            std::exchange(*current, WTFMove(replacement));
        });
    }

    template<typename T>
    std::enable_if_t<std::is_fundamental_v<T> || std::is_enum_v<T>, void> replace(T* current, T&& replacement)
    {
        std::swap(*current, replacement);
        m_replacements.append([current, replacement = WTFMove(replacement)]() mutable {
            std::exchange(*current, WTFMove(replacement));
        });
    }

    template<typename CurrentType, typename ReplacementType>
    std::enable_if_t<sizeof(CurrentType) < sizeof(ReplacementType) || std::is_same_v<ReplacementType, AST::Expression>, void> replace(CurrentType& current, ReplacementType& replacement)
    {
        m_replacements.append([&current, currentCopy = current]() mutable {
            bitwise_cast<AST::IdentityExpression*>(&current)->~IdentityExpression();
            new (&current) CurrentType(WTFMove(currentCopy));
        });

        current.~CurrentType();
        new (&current) AST::IdentityExpression(replacement.span(), replacement);
    }

    template<typename CurrentType, typename ReplacementType>
    std::enable_if_t<sizeof(CurrentType) >= sizeof(ReplacementType) && !std::is_same_v<ReplacementType, AST::Expression>, void> replace(CurrentType& current, ReplacementType& replacement)
    {
        m_replacements.append([&current, currentCopy = current]() mutable {
            bitwise_cast<ReplacementType*>(&current)->~ReplacementType();
            new (bitwise_cast<void*>(&current)) CurrentType(WTFMove(currentCopy));
        });

        current.~CurrentType();
        new (bitwise_cast<void*>(&current)) ReplacementType(replacement);
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

    template<typename T, typename U, size_t size>
    void append(const Vector<U, size>& constVector, T&& value)
    {
        auto& vector = const_cast<Vector<U, size>&>(constVector);
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

    template<typename T, size_t size, typename T2, size_t size2>
    void insertVector(const Vector<T, size>& constVector, size_t position, const Vector<T2, size2>& value)
    {
        auto& vector = const_cast<Vector<T, size>&>(constVector);
        vector.insertVector(position, value);
        m_replacements.append([&vector, position, length = value.size()]() {
            vector.remove(position, length);
        });
    }

    template<typename T, size_t size>
    void remove(const Vector<T, size>& constVector, size_t position)
    {
        auto& vector = const_cast<Vector<T, size>&>(constVector);
        auto entry = vector[position];
        m_replacements.append([&vector, position, entry]() mutable {
            vector.insert(position, entry);
        });
        vector.remove(position);
    }

    template<typename T, size_t size>
    void clear(const Vector<T, size>& constVector)
    {
        auto& vector = const_cast<Vector<T, size>&>(constVector);
        m_replacements.append([&vector, contents = WTFMove(vector)]() mutable {
            vector = contents;
        });
        vector.clear();
    }

    void revertReplacements()
    {
        for (int i = m_replacements.size() - 1; i >= 0; --i)
            m_replacements[i]();
        m_replacements.clear();
    }

    OptionSet<Extension>& enabledExtensions() { return m_enabledExtensions; }
    OptionSet<LanguageFeature> requiredFeatures() { return m_requiredFeatures; }
    bool containsOverride(uint32_t idValue) const
    {
        return m_pipelineOverrideIds.contains(idValue);
    }
    void addOverride(uint32_t idValue)
    {
        m_pipelineOverrideIds.add(idValue);
    }
    bool hasFeature(const String& featureName) const { return m_configuration.supportedFeatures.contains(featureName); }

private:
    String m_source;
    bool m_usesExternalTextures { false };
    bool m_usesPackArray { false };
    bool m_usesUnpackArray { false };
    bool m_usesWorkgroupUniformLoad { false };
    bool m_usesDivision { false };
    bool m_usesModulo { false };
    bool m_usesFrexp { false };
    bool m_usesModf { false };
    bool m_usesAtomicCompareExchange { false };
    bool m_usesFragDepth { false };
    bool m_usesDot { false };
    bool m_usesFirstLeadingBit { false };
    bool m_usesFirstTrailingBit { false };
    bool m_usesSign { false };
    bool m_usesSampleMask { false };
    bool m_usesFrontFacing { false };
    bool m_usesSampleIndex { false };
    bool m_usesDot4I8Packed { false };
    bool m_usesDot4U8Packed { false };
    bool m_usesExtractBits { false };
    OptionSet<Extension> m_enabledExtensions;
    OptionSet<LanguageFeature> m_requiredFeatures;
    Configuration m_configuration;
    AST::Declaration::List m_declarations;
    AST::Directive::List m_directives;
    TypeStore m_types;
    AST::Builder m_astBuilder;
    Vector<std::function<void()>> m_replacements;
    HashSet<uint32_t, DefaultHash<uint32_t>, WTF::UnsignedWithZeroKeyHashTraits<uint32_t>> m_pipelineOverrideIds;
};

} // namespace WGSL
