/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
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
#include "SelectPopoverElement.h"

#include "Document.h"
#include "HTMLSelectElement.h"
#include "RenderStyle.h"
#include "ShadowRoot.h"
#include "StyleAppearance.h"
#include "RenderStyle+GettersInlines.h"
#include <wtf/TZoneMallocInlines.h>

namespace WebCore {

WTF_MAKE_TZONE_ALLOCATED_IMPL(SelectPopoverElement);

Ref<SelectPopoverElement> SelectPopoverElement::create(Document& document)
{
    return adoptRef(*new SelectPopoverElement(document));
}

SelectPopoverElement::SelectPopoverElement(Document& document)
    : HTMLDivElement(document, TypeFlag::HasCustomStyleResolveCallbacks)
{
}

HTMLSelectElement* SelectPopoverElement::selectElement() const
{
    auto* shadowRoot = containingShadowRoot();
    if (!shadowRoot)
        return nullptr;
    return dynamicDowncast<HTMLSelectElement>(shadowRoot->host());
}

void SelectPopoverElement::didAttachRenderers()
{
    HTMLDivElement::didAttachRenderers();

    CheckedPtr style = computedStyle();
    bool newIsAppearanceBase = style && style->usedAppearance() == StyleAppearance::Base;

    if (m_wasBaseAppearancePicker && !newIsAppearanceBase) {
        if (RefPtr select = selectElement(); select && select->popupIsVisible())
            select->queuePickerCloseForAppearanceChange();
    }

    m_wasBaseAppearancePicker = newIsAppearanceBase;
}

void SelectPopoverElement::popoverWasHidden()
{
    if (RefPtr select = selectElement()) {
        select->setPopupIsVisible(false);
        select->focus();
    }
}

} // namespace WebCore
