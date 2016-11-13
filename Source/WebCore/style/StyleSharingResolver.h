/*
 * Copyright (C) 2016 Apple Inc. All rights reserved.
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

#include <wtf/HashMap.h>

namespace WebCore {

class Document;
class DocumentRuleSets;
class Element;
class Node;
class RenderStyle;
class RuleSet;
class SelectorFilter;
class SpaceSplitString;
class StyledElement;

namespace Style {

class Update;

class SharingResolver {
public:
    SharingResolver(const Document&, const DocumentRuleSets&, const SelectorFilter&);

    std::unique_ptr<RenderStyle> resolve(const Element&, const Update&);

private:
    struct Context;

    StyledElement* findSibling(const Context&, Node*, unsigned& count) const;
    Node* locateCousinList(const Element* parent) const;
    bool canShareStyleWithElement(const Context&, const StyledElement& candidateElement) const;
    bool styleSharingCandidateMatchesRuleSet(const StyledElement&, const RuleSet*) const;
    bool sharingCandidateHasIdenticalStyleAffectingAttributes(const Context&, const StyledElement& sharingCandidate) const;
    bool classNamesAffectedByRules(const SpaceSplitString& classNames) const;

    const Document& m_document;
    const DocumentRuleSets& m_ruleSets;
    const SelectorFilter& m_selectorFilter;

    HashMap<const Element*, const Element*> m_elementsSharingStyle;
};

}
}
