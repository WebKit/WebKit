/*
 * Copyright (C) 2012-2019 Apple Inc. All rights reserved.
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

#include <JavaScriptCore/UnlinkedCodeBlock.h>

namespace JSC {

template<typename CodeBlockType>
class CachedGlobalCodeBlock;

class UnlinkedGlobalCodeBlock : public UnlinkedCodeBlock {
public:
    typedef UnlinkedCodeBlock Base;

    void recordParse(CodeFeatures features, LexicallyScopedFeatures lexicallyScopedFeatures, bool hasCapturedVariables, unsigned lineCount, unsigned endColumn)
    {
        m_features = features;
        m_lexicallyScopedFeatures = lexicallyScopedFeatures;
        m_hasCapturedVariables = hasCapturedVariables;
        m_lineCount = lineCount;
        // For the UnlinkedCodeBlock, startColumn is always 0.
        m_endColumn = endColumn;
    }

    StringImpl* sourceURLDirective() const { return m_sourceURLDirective.get(); }
    StringImpl* sourceMappingURLDirective() const { return m_sourceMappingURLDirective.get(); }
    void setSourceURLDirective(const String& sourceURL) { m_sourceURLDirective = sourceURL.impl(); }
    void setSourceMappingURLDirective(const String& sourceMappingURL) { m_sourceMappingURLDirective = sourceMappingURL.impl(); }

    CodeFeatures codeFeatures() const { return m_features; }
    bool allowDirectEvalCache() const { return !(m_features & NoEvalCacheFeature); }
    LexicallyScopedFeatures lexicallyScopedFeatures() const { return m_lexicallyScopedFeatures; }
    bool hasCapturedVariables() const { return m_hasCapturedVariables; }
    unsigned lineCount() const { return m_lineCount; }
    ALWAYS_INLINE unsigned startColumn() const { return 0; }
    unsigned endColumn() const { return m_endColumn; }

protected:
    UnlinkedGlobalCodeBlock(VM& vm, Structure* structure, CodeType codeType, const ExecutableInfo& info, OptionSet<CodeGenerationMode> codeGenerationMode)
        : Base(vm, structure, codeType, info, codeGenerationMode)
    {
    }

    template<typename CodeBlockType>
    UnlinkedGlobalCodeBlock(Decoder& decoder, Structure* structure, const CachedCodeBlock<CodeBlockType>& cachedCodeBlock)
        : Base(decoder, structure, cachedCodeBlock)
    {
    }

private:
    template<typename CodeBlockType>
    friend class CachedGlobalCodeBlock;

    CodeFeatures m_features { NoFeatures };
    LexicallyScopedFeatures m_lexicallyScopedFeatures { NoLexicallyScopedFeatures };
    bool m_hasCapturedVariables { false };
    unsigned m_lineCount { 0 };
    unsigned m_endColumn { UINT_MAX };

    PackedRefPtr<StringImpl> m_sourceURLDirective;
    PackedRefPtr<StringImpl> m_sourceMappingURLDirective;
};

}
