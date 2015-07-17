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

#ifndef SourceProviderCacheItem_h
#define SourceProviderCacheItem_h

#include "ParserTokens.h"
#include <wtf/Vector.h>
#include <wtf/text/UniquedStringImpl.h>
#include <wtf/text/WTFString.h>

namespace JSC {

struct SourceProviderCacheItemCreationParameters {
    unsigned functionNameStart;
    unsigned lastTockenLine;
    unsigned lastTockenStartOffset;
    unsigned lastTockenEndOffset;
    unsigned lastTockenLineStartOffset;
    unsigned endFunctionOffset;
    unsigned parameterCount;
    bool needsFullActivation;
    bool usesEval;
    bool strictMode;
    Vector<RefPtr<UniquedStringImpl>> usedVariables;
    Vector<RefPtr<UniquedStringImpl>> writtenVariables;
    bool isBodyArrowExpression { false };
    JSTokenType tokenType { CLOSEBRACE };
};

#if COMPILER(MSVC)
#pragma warning(push)
#pragma warning(disable: 4200) // Disable "zero-sized array in struct/union" warning
#endif

class SourceProviderCacheItem {
    WTF_MAKE_FAST_ALLOCATED;
public:
    static std::unique_ptr<SourceProviderCacheItem> create(const SourceProviderCacheItemCreationParameters&);
    ~SourceProviderCacheItem();

    JSToken endFunctionToken() const 
    {
        JSToken token;
        token.m_type = isBodyArrowExpression ? tokenType : CLOSEBRACE;
        token.m_data.offset = lastTockenStartOffset;
        token.m_location.startOffset = lastTockenStartOffset;
        token.m_location.endOffset = lastTockenEndOffset;
        token.m_location.line = lastTockenLine;
        token.m_location.lineStartOffset = lastTockenLineStartOffset;
        // token.m_location.sourceOffset is initialized once by the client. So,
        // we do not need to set it here.
        return token;
    }

    unsigned functionNameStart : 31;
    bool needsFullActivation : 1;

    unsigned endFunctionOffset : 31;
    unsigned lastTockenLine : 31;
    unsigned lastTockenStartOffset : 31;
    unsigned lastTockenEndOffset: 31;
    unsigned parameterCount;
    
    bool usesEval : 1;

    bool strictMode : 1;

    unsigned lastTockenLineStartOffset;
    unsigned usedVariablesCount;
    unsigned writtenVariablesCount;

    UniquedStringImpl** usedVariables() const { return const_cast<UniquedStringImpl**>(m_variables); }
    UniquedStringImpl** writtenVariables() const { return const_cast<UniquedStringImpl**>(&m_variables[usedVariablesCount]); }
    bool isBodyArrowExpression;
    JSTokenType tokenType;

private:
    SourceProviderCacheItem(const SourceProviderCacheItemCreationParameters&);

    UniquedStringImpl* m_variables[0];
};

inline SourceProviderCacheItem::~SourceProviderCacheItem()
{
    for (unsigned i = 0; i < usedVariablesCount + writtenVariablesCount; ++i)
        m_variables[i]->deref();
}

inline std::unique_ptr<SourceProviderCacheItem> SourceProviderCacheItem::create(const SourceProviderCacheItemCreationParameters& parameters)
{
    size_t variableCount = parameters.writtenVariables.size() + parameters.usedVariables.size();
    size_t objectSize = sizeof(SourceProviderCacheItem) + sizeof(UniquedStringImpl*) * variableCount;
    void* slot = fastMalloc(objectSize);
    return std::unique_ptr<SourceProviderCacheItem>(new (slot) SourceProviderCacheItem(parameters));
}

inline SourceProviderCacheItem::SourceProviderCacheItem(const SourceProviderCacheItemCreationParameters& parameters)
    : functionNameStart(parameters.functionNameStart)
    , needsFullActivation(parameters.needsFullActivation)
    , endFunctionOffset(parameters.endFunctionOffset)
    , lastTockenLine(parameters.lastTockenLine)
    , lastTockenStartOffset(parameters.lastTockenStartOffset)
    , lastTockenEndOffset(parameters.lastTockenEndOffset)
    , parameterCount(parameters.parameterCount)
    , usesEval(parameters.usesEval)
    , strictMode(parameters.strictMode)
    , lastTockenLineStartOffset(parameters.lastTockenLineStartOffset)
    , usedVariablesCount(parameters.usedVariables.size())
    , writtenVariablesCount(parameters.writtenVariables.size())
    , isBodyArrowExpression(parameters.isBodyArrowExpression)
    , tokenType(parameters.tokenType)
{
    unsigned j = 0;
    for (unsigned i = 0; i < usedVariablesCount; ++i, ++j) {
        m_variables[j] = parameters.usedVariables[i].get();
        m_variables[j]->ref();
    }
    for (unsigned i = 0; i < writtenVariablesCount; ++i, ++j) {
        m_variables[j] = parameters.writtenVariables[i].get();
        m_variables[j]->ref();
    }
}

#if COMPILER(MSVC)
#pragma warning(pop)
#endif

}

#endif // SourceProviderCacheItem_h
