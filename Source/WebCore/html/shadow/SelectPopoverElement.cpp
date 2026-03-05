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
#include "EventLoop.h"
#include "HTMLSelectElement.h"
#include "RenderStyle.h"
#include "ShadowRoot.h"
#include <JavaScriptCore/ConsoleTypes.h>
#include <wtf/TZoneMallocInlines.h>

namespace WebCore {

WTF_MAKE_TZONE_ALLOCATED_IMPL(SelectPopoverElement);

Ref<SelectPopoverElement> SelectPopoverElement::create(Document& document)
{
    return adoptRef(*new SelectPopoverElement(document));
}

SelectPopoverElement::SelectPopoverElement(Document& document)
    : HTMLDivElement(document)
{
}

HTMLSelectElement* SelectPopoverElement::selectElement() const
{
    RefPtr shadowRoot = containingShadowRoot();
    if (!shadowRoot)
        return nullptr;
    return dynamicDowncast<HTMLSelectElement>(shadowRoot->host());
}

void SelectPopoverElement::didRecalcStyle(OptionSet<Style::Change> change)
{
    HTMLDivElement::didRecalcStyle(change);

    CheckedPtr style = computedStyle();
    if (!style)
        return;

    auto usedAppearance = style->usedAppearance();
    bool newIsAppearanceBase = (usedAppearance == StyleAppearance::Base);

    RefPtr select = selectElement();
    if (!select) {
        m_isAppearanceBase = newIsAppearanceBase;
        return;
    }

#if !PLATFORM(IOS_FAMILY)
    if (m_isAppearanceBase != newIsAppearanceBase && select->popupIsVisible()) {
        protect(protect(document())->eventLoop())->queueTask(TaskSource::DOMManipulation, [weakSelect = WeakPtr { select }] {
            RefPtr select = weakSelect.get();
            if (!select)
                return;
            protect(select->document())->addConsoleMessage(MessageSource::Other, MessageLevel::Warning,
                "The select element's appearance property changed while its picker was open. The picker has been closed."_s);
            select->hidePopup();
        });
    }
#endif

    m_isAppearanceBase = newIsAppearanceBase;
}

void SelectPopoverElement::popoverWasHidden()
{
    if (RefPtr select = selectElement()) {
        select->setPopupIsVisible(false);
        select->focus();
    }
}

} // namespace WebCore
