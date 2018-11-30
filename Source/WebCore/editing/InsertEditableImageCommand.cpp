/*
 * Copyright (C) 2018 Apple Inc. All rights reserved.
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
 */

#include "config.h"
#include "InsertEditableImageCommand.h"

#include "Editing.h"
#include "HTMLImageElement.h"

namespace WebCore {

RefPtr<HTMLImageElement> InsertEditableImageCommand::insertEditableImage(Document& document)
{
    RefPtr<InsertEditableImageCommand> insertCommand = create(document);
    insertCommand->apply();
    return insertCommand->m_imageElement;
}

InsertEditableImageCommand::InsertEditableImageCommand(Document& document)
    : CompositeEditCommand(document)
{
}

void InsertEditableImageCommand::doApply()
{
    if (endingSelection().isNone())
        return;

    m_imageElement = HTMLImageElement::create(document());
    m_imageElement->setAttributeWithoutSynchronization(x_apple_editable_imageAttr, emptyAtom());
    m_imageElement->setAttributeWithoutSynchronization(widthAttr, AtomicString("100%", AtomicString::ConstructFromLiteral));
    m_imageElement->setAttributeWithoutSynchronization(heightAttr, AtomicString("300px", AtomicString::ConstructFromLiteral));
    m_imageElement->setAttributeWithoutSynchronization(styleAttr, AtomicString("display: block", AtomicString::ConstructFromLiteral));

    insertNodeAt(*m_imageElement, endingSelection().start());
    setEndingSelection(visiblePositionAfterNode(*m_imageElement));
}

}
