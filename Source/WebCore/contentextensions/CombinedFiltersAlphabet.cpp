/*
 * Copyright (C) 2015 Apple Inc. All rights reserved.
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
#include "CombinedFiltersAlphabet.h"

#if ENABLE(CONTENT_EXTENSIONS)

namespace WebCore {

namespace ContentExtensions {

struct TermCreatorInput {
    const Term& term;
    Vector<std::unique_ptr<Term>>& internedTermsStorage;
};

struct TermCreatorTranslator {
    static unsigned hash(const TermCreatorInput& input)
    {
        return input.term.hash();
    }

    static inline bool equal(const Term* term, const TermCreatorInput& input)
    {
        return *term == input.term;
    }

    static void translate(const Term*& location, const TermCreatorInput& input, unsigned)
    {
        std::unique_ptr<Term> newUniqueTerm(new Term(input.term));
        location = newUniqueTerm.get();
        input.internedTermsStorage.append(WTFMove(newUniqueTerm));
    }
};

const Term* CombinedFiltersAlphabet::interned(const Term& term)
{
    TermCreatorInput input { term, m_internedTermsStorage };
    auto addResult = m_uniqueTerms.add<TermCreatorTranslator>(input);
    return *addResult.iterator;
}

#if CONTENT_EXTENSIONS_PERFORMANCE_REPORTING
size_t CombinedFiltersAlphabet::memoryUsed() const
{
    size_t termsSize = 0;
    for (const auto& termPointer : m_internedTermsStorage)
        termsSize += termPointer->memoryUsed();
    return sizeof(CombinedFiltersAlphabet)
        + termsSize
        + m_uniqueTerms.capacity() * sizeof(Term*)
        + m_internedTermsStorage.capacity() * sizeof(std::unique_ptr<Term>);
}
#endif

} // namespace ContentExtensions
} // namespace WebCore

#endif // ENABLE(CONTENT_EXTENSIONS)
