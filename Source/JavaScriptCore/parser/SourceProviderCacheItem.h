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
#include <wtf/PassOwnPtr.h>
#include <wtf/Vector.h>
#include <wtf/text/WTFString.h>

namespace JSC {

struct SourceProviderCacheItemCreationParameters {
    unsigned functionStart;
    unsigned closeBraceLine;
    unsigned closeBracePos;
    bool needsFullActivation;
    bool usesEval;
    bool strictMode;
    Vector<RefPtr<StringImpl> > usedVariables;
    Vector<RefPtr<StringImpl> > writtenVariables;
};

#if COMPILER(MSVC)
#pragma warning(push)
#pragma warning(disable: 4200) // Disable "zero-sized array in struct/union" warning
#endif

class SourceProviderCacheItem {
    WTF_MAKE_FAST_ALLOCATED;
public:
    static PassOwnPtr<SourceProviderCacheItem> create(const SourceProviderCacheItemCreationParameters&);
    ~SourceProviderCacheItem();

    unsigned approximateByteSize() const
    {
        // The identifiers are uniqued strings so most likely there are few names that actually use any additional memory.
        static const unsigned assumedAverageIdentifierSize = sizeof(StringImpl*) + 2;
        unsigned size = sizeof(*this);
        size += usedVariablesCount * assumedAverageIdentifierSize;
        size += writtenVariablesCount * assumedAverageIdentifierSize;
        return size;
    }
    JSToken closeBraceToken() const 
    {
        JSToken token;
        token.m_type = CLOSEBRACE;
        token.m_data.intValue = closeBracePos;
        token.m_location.startOffset = closeBracePos;
        token.m_location.endOffset = closeBracePos + 1;
        token.m_location.line = closeBraceLine;
        return token;
    }

    unsigned functionStart : 31;
    bool needsFullActivation : 1;
    
    unsigned closeBraceLine : 31;
    bool usesEval : 1;

    unsigned closeBracePos : 31;
    bool strictMode : 1;

    unsigned usedVariablesCount;
    unsigned writtenVariablesCount;

    StringImpl** usedVariables() const { return const_cast<StringImpl**>(m_variables); }
    StringImpl** writtenVariables() const { return const_cast<StringImpl**>(&m_variables[usedVariablesCount]); }

private:
    SourceProviderCacheItem(const SourceProviderCacheItemCreationParameters&);

    StringImpl* m_variables[0];
};

inline SourceProviderCacheItem::~SourceProviderCacheItem()
{
    for (unsigned i = 0; i < usedVariablesCount + writtenVariablesCount; ++i)
        m_variables[i]->deref();
}

inline PassOwnPtr<SourceProviderCacheItem> SourceProviderCacheItem::create(const SourceProviderCacheItemCreationParameters& parameters)
{
    size_t variableCount = parameters.writtenVariables.size() + parameters.usedVariables.size();
    size_t objectSize = sizeof(SourceProviderCacheItem) + sizeof(StringImpl*) * variableCount;
    void* slot = fastMalloc(objectSize);
    return adoptPtr(new (slot) SourceProviderCacheItem(parameters));
}

inline SourceProviderCacheItem::SourceProviderCacheItem(const SourceProviderCacheItemCreationParameters& parameters)
    : functionStart(parameters.functionStart)
    , needsFullActivation(parameters.needsFullActivation)
    , closeBraceLine(parameters.closeBraceLine)
    , usesEval(parameters.usesEval)
    , closeBracePos(parameters.closeBracePos)
    , strictMode(parameters.strictMode)
    , usedVariablesCount(parameters.usedVariables.size())
    , writtenVariablesCount(parameters.writtenVariables.size())
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
