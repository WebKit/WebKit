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

#include "config.h"
#include "SelectRuleFeatureSet.h"

#include "CSSSelector.h"

namespace WebCore {

SelectRuleFeatureSet::SelectRuleFeatureSet()
    : m_featureFlags(0)
{
}

void SelectRuleFeatureSet::add(const SelectRuleFeatureSet& featureSet)
{
    m_cssRuleFeatureSet.add(featureSet.m_cssRuleFeatureSet);
    m_featureFlags |= featureSet.m_featureFlags;
}

void SelectRuleFeatureSet::clear()
{
    m_cssRuleFeatureSet.clear();
    m_featureFlags = 0;
}

void SelectRuleFeatureSet::collectFeaturesFromSelector(const CSSSelector* selector)
{
    m_cssRuleFeatureSet.collectFeaturesFromSelector(selector);

    switch (selector->pseudoType()) {
    case CSSSelector::PseudoChecked:
        setSelectRuleFeature(AffectedSelectorChecked);
        break;
    case CSSSelector::PseudoEnabled:
        setSelectRuleFeature(AffectedSelectorEnabled);
        break;
    case CSSSelector::PseudoDisabled:
        setSelectRuleFeature(AffectedSelectorDisabled);
        break;
    case CSSSelector::PseudoIndeterminate:
        setSelectRuleFeature(AffectedSelectorIndeterminate);
        break;
    case CSSSelector::PseudoLink:
        setSelectRuleFeature(AffectedSelectorLink);
        break;
    case CSSSelector::PseudoTarget:
        setSelectRuleFeature(AffectedSelectorTarget);
        break;
    case CSSSelector::PseudoVisited:
        setSelectRuleFeature(AffectedSelectorVisited);
        break;
    default:
        break;
    }
}

}

