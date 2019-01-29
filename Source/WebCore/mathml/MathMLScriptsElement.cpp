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
 *
 */

#include "config.h"
#include "MathMLScriptsElement.h"

#if ENABLE(MATHML)

#include "RenderMathMLScripts.h"
#include <wtf/IsoMallocInlines.h>

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(MathMLScriptsElement);

using namespace MathMLNames;

static MathMLScriptsElement::ScriptType scriptTypeOf(const QualifiedName& tagName)
{
    if (tagName == msubTag)
        return MathMLScriptsElement::ScriptType::Sub;
    if (tagName == msupTag)
        return MathMLScriptsElement::ScriptType::Super;
    if (tagName == msubsupTag)
        return MathMLScriptsElement::ScriptType::SubSup;
    if (tagName == munderTag)
        return MathMLScriptsElement::ScriptType::Under;
    if (tagName == moverTag)
        return MathMLScriptsElement::ScriptType::Over;
    if (tagName == munderoverTag)
        return MathMLScriptsElement::ScriptType::UnderOver;
    ASSERT(tagName == mmultiscriptsTag);
    return MathMLScriptsElement::ScriptType::Multiscripts;
}

MathMLScriptsElement::MathMLScriptsElement(const QualifiedName& tagName, Document& document)
    : MathMLPresentationElement(tagName, document)
    , m_scriptType(scriptTypeOf(tagName))
{
}

Ref<MathMLScriptsElement> MathMLScriptsElement::create(const QualifiedName& tagName, Document& document)
{
    return adoptRef(*new MathMLScriptsElement(tagName, document));
}

const MathMLElement::Length& MathMLScriptsElement::subscriptShift()
{
    return cachedMathMLLength(subscriptshiftAttr, m_subscriptShift);
}

const MathMLElement::Length& MathMLScriptsElement::superscriptShift()
{
    return cachedMathMLLength(superscriptshiftAttr, m_superscriptShift);
}

void MathMLScriptsElement::parseAttribute(const QualifiedName& name, const AtomicString& value)
{
    if (name == subscriptshiftAttr)
        m_subscriptShift = WTF::nullopt;
    else if (name == superscriptshiftAttr)
        m_superscriptShift = WTF::nullopt;

    MathMLElement::parseAttribute(name, value);
}

RenderPtr<RenderElement> MathMLScriptsElement::createElementRenderer(RenderStyle&& style, const RenderTreePosition&)
{
    ASSERT(hasTagName(msubTag) || hasTagName(msupTag) || hasTagName(msubsupTag) || hasTagName(mmultiscriptsTag));
    return createRenderer<RenderMathMLScripts>(*this, WTFMove(style));
}

}

#endif // ENABLE(MATHML)
