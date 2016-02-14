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

#ifndef ClassChangeInvalidation_h
#define ClassChangeInvalidation_h

#include "Document.h"
#include "Element.h"
#include "StyleResolver.h"
#include <wtf/Vector.h>

namespace WebCore {

class DocumentRuleSets;
class SpaceSplitString;

namespace Style {

class ClassChangeInvalidation {
public:
    ClassChangeInvalidation(Element&, const SpaceSplitString& oldClasses, const SpaceSplitString& newClasses);
    ~ClassChangeInvalidation();

private:
    using ClassChangeVector = Vector<AtomicStringImpl*, 4>;

    static bool needsInvalidation(const Element&);
    void computeClassChange(const SpaceSplitString& oldClasses, const SpaceSplitString& newClasses);
    void invalidateStyle(const ClassChangeVector&);

    static ClassChangeVector collectClasses(const SpaceSplitString&);

    const bool m_isEnabled;
    Element& m_element;

    ClassChangeVector m_addedClasses;
    ClassChangeVector m_removedClasses;
};

inline bool ClassChangeInvalidation::needsInvalidation(const Element& element)
{
    if (!element.inRenderedDocument())
        return false;
    if (element.styleChangeType() >= FullStyleChange)
        return false;
    if (!element.document().styleResolverIfExists())
        return false;
    return true;
}

inline ClassChangeInvalidation::ClassChangeInvalidation(Element& element, const SpaceSplitString& oldClasses, const SpaceSplitString& newClasses)
    : m_isEnabled(needsInvalidation(element))
    , m_element(element)

{
    if (!m_isEnabled)
        return;
    computeClassChange(oldClasses, newClasses);
    invalidateStyle(m_removedClasses);
}

inline ClassChangeInvalidation::~ClassChangeInvalidation()
{
    if (!m_isEnabled)
        return;
    invalidateStyle(m_addedClasses);
}
    
}
}

#endif

