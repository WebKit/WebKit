/*
 * Copyright (C) 2021 Apple Inc. All rights reserved.
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
#include "ModalContainerObserver.h"

#include "Chrome.h"
#include "ChromeClient.h"
#include "Document.h"
#include "DocumentLoader.h"
#include "Frame.h"
#include "FrameView.h"
#include "HTMLBodyElement.h"
#include "HTMLDocument.h"
#include "HTMLElement.h"
#include "Page.h"
#include "RenderDescendantIterator.h"
#include "RenderText.h"
#include "RenderView.h"
#include "Text.h"
#include <wtf/URL.h>

namespace WebCore {

bool ModalContainerObserver::isNeededFor(const Document& document)
{
    RefPtr documentLoader = document.loader();
    if (!documentLoader || documentLoader->modalContainerObservationPolicy() == ModalContainerObservationPolicy::Disabled)
        return false;

    if (!document.url().protocolIsInHTTPFamily())
        return false;

    if (!document.isTopDocument()) {
        // FIXME (234446): Need to properly support subframes.
        return false;
    }

    if (document.inDesignMode() || !is<HTMLDocument>(document))
        return false;

    auto* frame = document.frame();
    if (!frame)
        return false;

    auto* page = frame->page();
    if (!page || page->isEditable())
        return false;

    return true;
}

ModalContainerObserver::ModalContainerObserver() = default;
ModalContainerObserver::~ModalContainerObserver() = default;

static bool matchesSearchTerm(const Text& textNode, const AtomString& searchTerm)
{
    RefPtr parent = textNode.parentElementInComposedTree();
    bool shouldSearchNode = ([&] {
        if (!is<HTMLElement>(parent.get()))
            return false;

        return parent->hasTagName(HTMLNames::aTag) || parent->hasTagName(HTMLNames::divTag) || parent->hasTagName(HTMLNames::pTag) || parent->hasTagName(HTMLNames::spanTag) || parent->hasTagName(HTMLNames::sectionTag);
    })();

    if (!shouldSearchNode)
        return false;

    if (LIKELY(!textNode.data().containsIgnoringASCIICase(searchTerm)))
        return false;

    return true;
}

void ModalContainerObserver::updateModalContainerIfNeeded(const FrameView& view)
{
    if (m_container)
        return;

    if (!view.hasViewportConstrainedObjects())
        return;

    auto* page = view.frame().page();
    if (!page)
        return;

    auto& searchTerm = page->chrome().client().searchStringForModalContainerObserver();
    if (searchTerm.isNull())
        return;

    for (auto& renderer : *view.viewportConstrainedObjects()) {
        RefPtr element = renderer.element();
        if (!element || is<HTMLBodyElement>(*element) || element->isDocumentNode())
            continue;

        if (!m_elementsToIgnoreWhenSearching.add(*element).isNewEntry)
            continue;

        for (auto& textRenderer : descendantsOfType<RenderText>(renderer)) {
            if (RefPtr textNode = textRenderer.textNode(); textNode && matchesSearchTerm(*textNode, searchTerm)) {
                m_container = element.get();
                return;
            }
        }
    }
}

} // namespace WebCore
