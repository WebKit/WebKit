// Copyright 2015 The Chromium Authors. All rights reserved.
// Copyright (C) 2016 Apple Inc. All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//    * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//    * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//    * Neither the name of Google Inc. nor the names of its
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#pragma once

#include "CSSParserToken.h"
#include "CSSParserTokenRange.h"
#include "CSSRegisteredCustomProperty.h"
#include <wtf/HashSet.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

class CSSParserTokenRange;

class CSSVariableData : public RefCounted<CSSVariableData> {
    WTF_MAKE_NONCOPYABLE(CSSVariableData);
    WTF_MAKE_FAST_ALLOCATED;
public:
    static Ref<CSSVariableData> create(const CSSParserTokenRange& range, bool needsVariableResolution = true)
    {
        return adoptRef(*new CSSVariableData(range, needsVariableResolution));
    }

    static Ref<CSSVariableData> createResolved(const Vector<CSSParserToken>& resolvedTokens, const CSSVariableData& unresolvedData)
    {
        return adoptRef(*new CSSVariableData(resolvedTokens, unresolvedData.m_backingString));
    }

    CSSParserTokenRange tokenRange() { return m_tokens; }

    const Vector<CSSParserToken>& tokens() const { return m_tokens; }

    bool operator==(const CSSVariableData& other) const;

    bool needsVariableResolution() const { return m_needsVariableResolution; }

    bool checkVariablesForCycles(const AtomicString& name, const RenderStyle&, HashSet<AtomicString>& seenProperties, HashSet<AtomicString>& invalidProperties) const;

    RefPtr<CSSVariableData> resolveVariableReferences(const CSSRegisteredCustomPropertySet&, const RenderStyle&) const;
    bool resolveTokenRange(const CSSRegisteredCustomPropertySet&, CSSParserTokenRange, Vector<CSSParserToken>&, const RenderStyle&) const;

private:
    CSSVariableData(const CSSParserTokenRange&, bool needsVariableResolution);

    // We can safely copy the tokens (which have raw pointers to substrings) because
    // StylePropertySets contain references to CSSCustomPropertyValues, which
    // point to the unresolved CSSVariableData values that own the backing strings
    // this will potentially reference.
    CSSVariableData(const Vector<CSSParserToken>& resolvedTokens, String backingString)
        : m_backingString(backingString)
        , m_tokens(resolvedTokens)
        , m_needsVariableResolution(false)
    { }

    void consumeAndUpdateTokens(const CSSParserTokenRange&);
    template<typename CharacterType> void updateTokens(const CSSParserTokenRange&);
    
    bool checkVariablesForCyclesWithRange(CSSParserTokenRange, const RenderStyle&, HashSet<AtomicString>& seenProperties, HashSet<AtomicString>& invalidProperties) const;
    bool resolveVariableReference(const CSSRegisteredCustomPropertySet&, CSSParserTokenRange, Vector<CSSParserToken>&, const RenderStyle&) const;
    bool resolveVariableFallback(const CSSRegisteredCustomPropertySet&, CSSParserTokenRange, Vector<CSSParserToken>&, const RenderStyle&) const;

    String m_backingString;
    Vector<CSSParserToken> m_tokens;
    const bool m_needsVariableResolution;

    // FIXME-NEWPARSER: We want to cache StyleProperties once we support @apply.
};

} // namespace WebCore
