/*
 * Copyright (C) 2017 Apple Inc. All Rights Reserved.
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
 *
 */

#pragma once

#include "HTMLNames.h"
#include "QualifiedName.h"
#include "SVGNames.h"
#include "XLinkNames.h"
#include "XMLNSNames.h"
#include "XMLNames.h"
#include <wtf/HashSet.h>
#include <wtf/Ref.h>

#if ENABLE(MATHML)
#include "MathMLNames.h"
#endif


namespace WebCore {

class QualifiedNameCache {
    WTF_MAKE_FAST_ALLOCATED;
public:
    QualifiedNameCache() = default;

    Ref<QualifiedName::QualifiedNameImpl> getOrCreate(const QualifiedNameComponents&);
    void remove(QualifiedName::QualifiedNameImpl&);

private:
    static const int staticQualifiedNamesCount = HTMLNames::HTMLTagsCount + HTMLNames::HTMLAttrsCount
#if ENABLE(MATHML)
        + MathMLNames::MathMLTagsCount + MathMLNames::MathMLAttrsCount
#endif
        + SVGNames::SVGTagsCount + SVGNames::SVGAttrsCount
        + XLinkNames::XLinkAttrsCount
        + XMLNSNames::XMLNSAttrsCount
        + XMLNames::XMLAttrsCount;

    struct QualifiedNameHashTraits : public HashTraits<QualifiedName::QualifiedNameImpl*> {
        static const int minimumTableSize = WTF::HashTableCapacityForSize<staticQualifiedNamesCount>::value;
    };

    using QNameSet = HashSet<QualifiedName::QualifiedNameImpl*, QualifiedNameHash, QualifiedNameHashTraits>;
    QNameSet m_cache;
};

}
