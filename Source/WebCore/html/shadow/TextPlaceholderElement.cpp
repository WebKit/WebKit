/*
 * Copyright (C) 2020 Apple Inc. All rights reserved.
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
#include "TextPlaceholderElement.h"

#include "HTMLNames.h"
#include "HTMLTextFormControlElement.h"
#include <wtf/IsoMallocInlines.h>

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(TextPlaceholderElement);

Ref<TextPlaceholderElement> TextPlaceholderElement::create(Document& document, const LayoutSize& size)
{
    return adoptRef(*new TextPlaceholderElement { document, size });
}

TextPlaceholderElement::TextPlaceholderElement(Document& document, const LayoutSize& size)
    : HTMLDivElement { HTMLNames::divTag, document }
{
    // FIXME: Move to User Agent stylesheet. See <https://webkit.org/b/208745>.
    setInlineStyleProperty(CSSPropertyDisplay, size.width() ? CSSValueInlineBlock : CSSValueBlock, false);
    setInlineStyleProperty(CSSPropertyVerticalAlign, CSSValueTop, false);
    setInlineStyleProperty(CSSPropertyVisibility, CSSValueHidden, true);
    setInlineStyleProperty(CSSPropertyWidth, size.width(), CSSUnitType::CSS_PX);
    setInlineStyleProperty(CSSPropertyHeight, size.height(), CSSUnitType::CSS_PX);
}

auto TextPlaceholderElement::insertedIntoAncestor(InsertionType insertionType, ContainerNode& parentOfInsertedTree) -> InsertedIntoAncestorResult
{
    if (insertionType.treeScopeChanged) {
        if (auto shadowHost = makeRefPtr(parentOfInsertedTree.shadowHost()); is<HTMLTextFormControlElement>(shadowHost))
            downcast<HTMLTextFormControlElement>(*shadowHost).setCanShowPlaceholder(false);
    }
    return HTMLDivElement::insertedIntoAncestor(insertionType, parentOfInsertedTree);
}

void TextPlaceholderElement::removedFromAncestor(RemovalType removalType, ContainerNode& oldParentOfRemovedTree)
{
    if (removalType.treeScopeChanged) {
        if (auto shadowHost = makeRefPtr(oldParentOfRemovedTree.shadowHost()); is<HTMLTextFormControlElement>(shadowHost))
            downcast<HTMLTextFormControlElement>(*shadowHost).setCanShowPlaceholder(true);
    }
    HTMLDivElement::removedFromAncestor(removalType, oldParentOfRemovedTree);
}

} // namespace WebCore
