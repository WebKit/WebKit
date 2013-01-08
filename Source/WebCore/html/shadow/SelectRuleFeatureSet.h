/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef SelectRuleFeatureSet_h
#define SelectRuleFeatureSet_h

#include "Element.h"
#include "RuleFeature.h"

namespace WebCore {

class SelectRuleFeatureSet {
public:
    SelectRuleFeatureSet();

    void add(const SelectRuleFeatureSet&);
    void clear();
    void collectFeaturesFromSelector(const CSSSelector*);

    bool hasSelectorForId(const AtomicString&) const;
    bool hasSelectorForClass(const AtomicString&) const;
    bool hasSelectorForAttribute(const AtomicString&) const;

    bool hasSelectorForChecked() const { return hasSelectorFor(AffectedSelectorChecked); }
    bool hasSelectorForEnabled() const { return hasSelectorFor(AffectedSelectorEnabled); }
    bool hasSelectorForDisabled() const { return hasSelectorFor(AffectedSelectorDisabled); }
    bool hasSelectorForIndeterminate() const { return hasSelectorFor(AffectedSelectorIndeterminate); }
    bool hasSelectorForLink() const { return hasSelectorFor(AffectedSelectorLink); }
    bool hasSelectorForTarget() const { return hasSelectorFor(AffectedSelectorTarget); }
    bool hasSelectorForVisited() const { return hasSelectorFor(AffectedSelectorVisited); }

    bool hasSelectorFor(AffectedSelectorMask features) const { return m_featureFlags & features; }

private:
    void setSelectRuleFeature(AffectedSelectorType feature) { m_featureFlags |= feature; }

    RuleFeatureSet m_cssRuleFeatureSet;
    int m_featureFlags;
};

inline bool SelectRuleFeatureSet::hasSelectorForId(const AtomicString& idValue) const
{
    ASSERT(!idValue.isEmpty());
    return m_cssRuleFeatureSet.idsInRules.contains(idValue.impl());
}

inline bool SelectRuleFeatureSet::hasSelectorForClass(const AtomicString& classValue) const
{
    ASSERT(!classValue.isEmpty());
    return m_cssRuleFeatureSet.classesInRules.contains(classValue.impl());
}

inline bool SelectRuleFeatureSet::hasSelectorForAttribute(const AtomicString& attributeName) const
{
    ASSERT(!attributeName.isEmpty());
    return m_cssRuleFeatureSet.attrsInRules.contains(attributeName.impl());
}

}

#endif
