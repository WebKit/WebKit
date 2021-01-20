/*
 * Copyright (C) 2015 Apple Inc. All rights reserved.
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

#include "config.h"
#include "RubyTextElement.h"

#include "RenderRuby.h"
#include "RenderRubyText.h"
#include "RenderTreePosition.h"
#include <wtf/IsoMallocInlines.h>

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(RubyTextElement);

using namespace HTMLNames;

RubyTextElement::RubyTextElement(const QualifiedName& tagName, Document& document)
    : HTMLElement(tagName, document)
{
    ASSERT(hasTagName(rtTag));
}

Ref<RubyTextElement> RubyTextElement::create(const QualifiedName& tagName, Document& document)
{
    return adoptRef(*new RubyTextElement(tagName, document));
}

Ref<RubyTextElement> RubyTextElement::create(Document& document)
{
    return adoptRef(*new RubyTextElement(rtTag, document));
}

RenderPtr<RenderElement> RubyTextElement::createElementRenderer(RenderStyle&& style, const RenderTreePosition& insertionPosition)
{
    // RenderRubyText requires its parent to be RenderRubyRun.
    if (isRuby(insertionPosition.parent()) && style.display() == DisplayType::Block)
        return createRenderer<RenderRubyText>(*this, WTFMove(style));
    return HTMLElement::createElementRenderer(WTFMove(style), insertionPosition);
}

}
