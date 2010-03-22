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

#if ENABLE(MATHML)

#include "MathMLInlineContainerElement.h"

#include "MathMLNames.h"
#include "RenderMathMLBlock.h"
#include "RenderMathMLFraction.h"
#include "RenderMathMLMath.h"
#include "RenderMathMLRow.h"
#include "RenderMathMLSubSup.h"
#include "RenderMathMLUnderOver.h"

namespace WebCore {
    
using namespace MathMLNames;

MathMLInlineContainerElement::MathMLInlineContainerElement(const QualifiedName& tagName, Document* document)
    : MathMLElement(tagName, document)
{
}

PassRefPtr<MathMLInlineContainerElement> MathMLInlineContainerElement::create(const QualifiedName& tagName, Document* document)
{
    return new MathMLInlineContainerElement(tagName, document);
}

RenderObject* MathMLInlineContainerElement::createRenderer(RenderArena *arena, RenderStyle* style)
{
    RenderObject* object = 0;
    if (hasLocalName(MathMLNames::mrowTag))
        object = new (arena) RenderMathMLRow(this);
    else if (hasLocalName(MathMLNames::mathTag))
        object = new (arena) RenderMathMLMath(this);
    else if (hasLocalName(MathMLNames::msubTag))
        object = new (arena) RenderMathMLSubSup(this);
    else if (hasLocalName(MathMLNames::msupTag))
        object = new (arena) RenderMathMLSubSup(this);
    else if (hasLocalName(MathMLNames::msubsupTag))
        object = new (arena) RenderMathMLSubSup(this);
    else if (hasLocalName(MathMLNames::moverTag))
        object = new (arena) RenderMathMLUnderOver(this);
    else if (hasLocalName(MathMLNames::munderTag))
        object = new (arena) RenderMathMLUnderOver(this);
    else if (hasLocalName(MathMLNames::munderoverTag))
        object = new (arena) RenderMathMLUnderOver(this);
    else if (hasLocalName(MathMLNames::mfracTag))
        object = new (arena) RenderMathMLFraction(this);
    else
        object = new (arena) RenderMathMLBlock(this);
    object->setStyle(style);
    return object;
}
    
    
}

#endif // ENABLE(MATHML)

