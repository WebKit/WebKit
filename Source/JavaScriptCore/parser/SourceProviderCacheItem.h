/*
 * Copyright (C) 2011 Apple Inc. All rights reserved.
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

#include "ParserModes.h"
#include "ParserTokens.h"
#include <wtf/Vector.h>
#include <wtf/text/UniquedStringImpl.h>
#include <wtf/text/WTFString.h>

namespace JSC {

struct SourceProviderCacheItemCreationParameters {
    unsigned lastTokenLine;
    unsigned lastTokenStartOffset;
    unsigned lastTokenEndOffset;
    unsigned lastTokenLineStartOffset;
    unsigned endFunctionOffset;
    unsigned parameterCount;
    bool needsFullActivation;
    bool usesEval;
    LexicalScopeFeatures lexicalScopeFeatures;
    bool needsSuperBinding;
    InnerArrowFunctionCodeFeatures innerArrowFunctionFeatures;
    Vector<UniquedStringImpl*, 8> usedVariables;
    bool isBodyArrowExpression { false };
    JSTokenType tokenType { CLOSEBRACE };
    ConstructorKind constructorKind;
    SuperBinding expectedSuperBinding;
};

#if COMPILER(MSVC)
#pragma warning(push)
#pragma warning(disable: 4200) // Disable "zero-sized array in struct/union" warning
#endif

DECLARE_ALLOCATOR_WITH_HEAP_IDENTIFIER(SourceProviderCacheItem);
class SourceProviderCacheItem {
    WTF_MAKE_STRUCT_FAST_ALLOCATED_WITH_HEAP_IDENTIFIER(SourceProviderCacheItem);
public:
    static std::unique_ptr<SourceProviderCacheItem> create(const SourceProviderCacheItemCreationParameters&);
    ~SourceProviderCacheItem();

    JSToken endFunctionToken() const 
    {
        JSToken token;
        token.m_type = isBodyArrowExpression ? static_cast<JSTokenType>(tokenType) : CLOSEBRACE;
        token.m_data.offset = lastTokenStartOffset;
        token.m_location.startOffset = lastTokenStartOffset;
        token.m_location.endOffset = lastTokenEndOffset;
        token.m_location.line = lastTokenLine;
        token.m_location.lineStartOffset = lastTokenLineStartOffset;
        // token.m_location.sourceOffset is initialized once by the client. So,
        // we do not need to set it here.
        return token;
    }

    LexicalScopeFeatures lexicalScopeFeatures() const
    {
        LexicalScopeFeatures features = NoLexicalFeatures;
        if (strictMode)
            features |= StrictModeLexicalFeature;
        return features;
    }

    bool needsFullActivation : 1;
    unsigned endFunctionOffset : 31;
    bool usesEval : 1;
    unsigned lastTokenLine : 31;
    bool strictMode : 1;
    unsigned lastTokenStartOffset : 31;
    unsigned expectedSuperBinding : 1; // SuperBinding
    unsigned lastTokenEndOffset: 31;
    bool needsSuperBinding: 1;
    unsigned parameterCount : 31;
    unsigned lastTokenLineStartOffset : 31;
    bool isBodyArrowExpression : 1;
    unsigned usedVariablesCount;
    unsigned tokenType : 24; // JSTokenType
    unsigned innerArrowFunctionFeatures : 6; // InnerArrowFunctionCodeFeatures
    unsigned constructorKind : 2; // ConstructorKind

    PackedPtr<UniquedStringImpl>* usedVariables() const { return const_cast<PackedPtr<UniquedStringImpl>*>(m_variables); }

private:
    SourceProviderCacheItem(const SourceProviderCacheItemCreationParameters&);

    PackedPtr<UniquedStringImpl> m_variables[0];
};

inline SourceProviderCacheItem::~SourceProviderCacheItem()
{
    for (unsigned i = 0; i < usedVariablesCount; ++i)
        m_variables[i]->deref();
}

inline std::unique_ptr<SourceProviderCacheItem> SourceProviderCacheItem::create(const SourceProviderCacheItemCreationParameters& parameters)
{
    size_t variableCount = parameters.usedVariables.size();
    size_t objectSize = sizeof(SourceProviderCacheItem) + sizeof(UniquedStringImpl*) * variableCount;
    void* slot = SourceProviderCacheItemMalloc::malloc(objectSize);
    return std::unique_ptr<SourceProviderCacheItem>(new (slot) SourceProviderCacheItem(parameters));
}

inline SourceProviderCacheItem::SourceProviderCacheItem(const SourceProviderCacheItemCreationParameters& parameters)
    : needsFullActivation(parameters.needsFullActivation)
    , endFunctionOffset(parameters.endFunctionOffset)
    , usesEval(parameters.usesEval)
    , lastTokenLine(parameters.lastTokenLine)
    , strictMode(parameters.lexicalScopeFeatures & StrictModeLexicalFeature)
    , lastTokenStartOffset(parameters.lastTokenStartOffset)
    , expectedSuperBinding(static_cast<unsigned>(parameters.expectedSuperBinding))
    , lastTokenEndOffset(parameters.lastTokenEndOffset)
    , needsSuperBinding(parameters.needsSuperBinding)
    , parameterCount(parameters.parameterCount)
    , lastTokenLineStartOffset(parameters.lastTokenLineStartOffset)
    , isBodyArrowExpression(parameters.isBodyArrowExpression)
    , usedVariablesCount(parameters.usedVariables.size())
    , tokenType(static_cast<unsigned>(parameters.tokenType))
    , innerArrowFunctionFeatures(static_cast<unsigned>(parameters.innerArrowFunctionFeatures))
    , constructorKind(static_cast<unsigned>(parameters.constructorKind))
{
    ASSERT(tokenType == static_cast<unsigned>(parameters.tokenType));
    ASSERT(innerArrowFunctionFeatures == static_cast<unsigned>(parameters.innerArrowFunctionFeatures));
    ASSERT(constructorKind == static_cast<unsigned>(parameters.constructorKind));
    ASSERT(expectedSuperBinding == static_cast<unsigned>(parameters.expectedSuperBinding));
    for (unsigned i = 0; i < usedVariablesCount; ++i) {
        auto* pointer = parameters.usedVariables[i];
        pointer->ref();
        m_variables[i] = pointer;
    }
}

#if COMPILER(MSVC)
#pragma warning(pop)
#endif

} // namespace JSC
