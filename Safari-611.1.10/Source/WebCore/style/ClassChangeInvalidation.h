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

#include "Element.h"
#include "StyleInvalidator.h"
#include <wtf/Vector.h>

namespace WebCore {

class SpaceSplitString;

namespace Style {

class ClassChangeInvalidation {
public:
    ClassChangeInvalidation(Element&, const SpaceSplitString& oldClasses, const SpaceSplitString& newClasses);
    ~ClassChangeInvalidation();

private:
    void computeInvalidation(const SpaceSplitString& oldClasses, const SpaceSplitString& newClasses);
    void invalidateStyleWithRuleSets();

    const bool m_isEnabled;
    Element& m_element;

    Invalidator::MatchElementRuleSets m_matchElementRuleSets;
};

inline ClassChangeInvalidation::ClassChangeInvalidation(Element& element, const SpaceSplitString& oldClasses, const SpaceSplitString& newClasses)
    : m_isEnabled(element.needsStyleInvalidation())
    , m_element(element)

{
    if (!m_isEnabled)
        return;
    computeInvalidation(oldClasses, newClasses);
    invalidateStyleWithRuleSets();
}

inline ClassChangeInvalidation::~ClassChangeInvalidation()
{
    if (!m_isEnabled)
        return;
    invalidateStyleWithRuleSets();
}

}
}
