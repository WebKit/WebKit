/*
 * Copyright (C) 2016 Igalia S.L. All rights reserved.
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
 */

#include "config.h"
#include "MathMLRowElement.h"

#if ENABLE(MATHML)

#include "MathMLNames.h"
#include "MathMLOperatorElement.h"
#include "RenderMathMLFenced.h"
#include "RenderMathMLMenclose.h"
#include "RenderMathMLRow.h"
#include "RenderStyleInlines.h"
#include <wtf/IsoMallocInlines.h>

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(MathMLRowElement);

using namespace MathMLNames;

MathMLRowElement::MathMLRowElement(const QualifiedName& tagName, Document& document, OptionSet<TypeFlag> constructionType)
    : MathMLPresentationElement(tagName, document, constructionType)
{
}

Ref<MathMLRowElement> MathMLRowElement::create(const QualifiedName& tagName, Document& document)
{
    return adoptRef(*new MathMLRowElement(tagName, document));
}

void MathMLRowElement::childrenChanged(const ChildChange& change)
{
    for (auto child = firstChild(); child; child = child->nextSibling()) {
        if (child->hasTagName(moTag))
            static_cast<MathMLOperatorElement*>(child)->setOperatorFormDirty();
    }

    MathMLPresentationElement::childrenChanged(change);
}

RenderPtr<RenderElement> MathMLRowElement::createElementRenderer(RenderStyle&& style, const RenderTreePosition&)
{
    if (hasTagName(mfencedTag))
        return createRenderer<RenderMathMLFenced>(*this, WTFMove(style));

    ASSERT(hasTagName(merrorTag) || hasTagName(mphantomTag) || hasTagName(mrowTag) || hasTagName(mstyleTag));
    return createRenderer<RenderMathMLRow>(RenderObject::Type::MathMLRow, *this, WTFMove(style));
}

bool MathMLRowElement::acceptsMathVariantAttribute()
{
    return hasTagName(mstyleTag);
}

}

#endif // ENABLE(MATHML)
