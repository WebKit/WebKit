/*
 * Copyright (C) 2017 Igalia S.L. All rights reserved.
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
 *
 */

#include "config.h"
#include "MathMLRootElement.h"

#if ENABLE(MATHML)

#include "RenderMathMLRoot.h"
#include <wtf/IsoMallocInlines.h>

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(MathMLRootElement);

using namespace MathMLNames;

static RootType rootTypeOf(const QualifiedName& tagName)
{
    if (tagName == msqrtTag)
        return RootType::SquareRoot;
    ASSERT(tagName == mrootTag);
    return RootType::RootWithIndex;
}

inline MathMLRootElement::MathMLRootElement(const QualifiedName& tagName, Document& document)
    : MathMLRowElement(tagName, document)
    , m_rootType(rootTypeOf(tagName))
{
}

Ref<MathMLRootElement> MathMLRootElement::create(const QualifiedName& tagName, Document& document)
{
    return adoptRef(*new MathMLRootElement(tagName, document));
}

RenderPtr<RenderElement> MathMLRootElement::createElementRenderer(RenderStyle&& style, const RenderTreePosition&)
{
    ASSERT(hasTagName(msqrtTag) || hasTagName(mrootTag));
    return createRenderer<RenderMathMLRoot>(*this, WTFMove(style));
}

}

#endif // ENABLE(MATHML)
