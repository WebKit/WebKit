/*
 * Copyright (C) 2009 Alex Milowski (alex@milowski.com). All rights reserved.
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
#include "RenderMathMLFenced.h"

#if ENABLE(MATHML)

#include "ElementInlines.h"
#include "FontSelector.h"
#include "MathMLNames.h"
#include "MathMLRowElement.h"
#include "RenderBoxInlines.h"
#include "RenderBoxModelObjectInlines.h"
#include "RenderInline.h"
#include "RenderMathMLFencedOperator.h"
#include "RenderText.h"
#include "RenderTreeBuilder.h"
#include <wtf/IsoMallocInlines.h>
#include <wtf/text/StringBuilder.h>

namespace WebCore {

using namespace MathMLNames;

WTF_MAKE_ISO_ALLOCATED_IMPL(RenderMathMLFenced);

static constexpr auto gOpeningBraceChar = "("_s;
static constexpr auto gClosingBraceChar = ")"_s;

RenderMathMLFenced::RenderMathMLFenced(MathMLRowElement& element, RenderStyle&& style)
    : RenderMathMLRow(Type::MathMLFenced, element, WTFMove(style))
{
    ASSERT(isRenderMathMLFenced());
}

void RenderMathMLFenced::updateFromElement()
{
    const auto& fenced = element();

    // The open operator defaults to a left parenthesis.
    auto& open = fenced.attributeWithoutSynchronization(MathMLNames::openAttr);
    m_open = open.isNull() ? gOpeningBraceChar : open;

    // The close operator defaults to a right parenthesis.
    auto& close = fenced.attributeWithoutSynchronization(MathMLNames::closeAttr);
    m_close = close.isNull() ? gClosingBraceChar : close;

    auto& separators = fenced.attributeWithoutSynchronization(MathMLNames::separatorsAttr);
    if (!separators.isNull()) {
        StringBuilder characters;
        for (unsigned i = 0; i < separators.length(); i++) {
            if (!deprecatedIsSpaceOrNewline(separators[i]))
                characters.append(separators[i]);
        }
        m_separators = !characters.length() ? 0 : characters.toString().impl();
    } else {
        // The separator defaults to a single comma.
        m_separators = StringImpl::create(","_s);
    }

    if (firstChild()) {
        // FIXME: The mfenced element fails to update dynamically when its open, close and separators attributes are changed (https://bugs.webkit.org/show_bug.cgi?id=57696).
        if (auto* fencedOperator = dynamicDowncast<RenderMathMLFencedOperator>(*firstChild()))
            fencedOperator->updateOperatorContent(m_open);
        m_closeFenceRenderer->updateOperatorContent(m_close);
    }
}

}

#endif
