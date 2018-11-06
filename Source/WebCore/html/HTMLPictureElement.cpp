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
#include "HTMLPictureElement.h"

#include "ElementChildIterator.h"
#include "HTMLAnchorElement.h"
#include "HTMLImageElement.h"
#include "Logging.h"
#include "RuntimeEnabledFeatures.h"
#include <wtf/IsoMallocInlines.h>

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(HTMLPictureElement);

HTMLPictureElement::HTMLPictureElement(const QualifiedName& tagName, Document& document)
    : HTMLElement(tagName, document)
{
}

HTMLPictureElement::~HTMLPictureElement()
{
    document().removeViewportDependentPicture(*this);
    document().removeAppearanceDependentPicture(*this);
}

void HTMLPictureElement::didMoveToNewDocument(Document& oldDocument, Document& newDocument)
{
    oldDocument.removeViewportDependentPicture(*this);
    oldDocument.removeAppearanceDependentPicture(*this);
    HTMLElement::didMoveToNewDocument(oldDocument, newDocument);
    sourcesChanged();
}

Ref<HTMLPictureElement> HTMLPictureElement::create(const QualifiedName& tagName, Document& document)
{
    return adoptRef(*new HTMLPictureElement(tagName, document));
}

void HTMLPictureElement::sourcesChanged()
{
    for (auto& element : childrenOfType<HTMLImageElement>(*this))
        element.selectImageSource();
}

bool HTMLPictureElement::viewportChangeAffectedPicture() const
{
    auto documentElement = makeRefPtr(document().documentElement());
    MediaQueryEvaluator evaluator { document().printing() ? "print" : "screen", document(), documentElement ? documentElement->computedStyle() : nullptr };
    for (auto& result : m_viewportDependentMediaQueryResults) {
        LOG(MediaQueries, "HTMLPictureElement %p viewportChangeAffectedPicture evaluating media queries", this);
        if (evaluator.evaluate(result.expression) != result.result)
            return true;
    }
    return false;
}

bool HTMLPictureElement::appearanceChangeAffectedPicture() const
{
    auto documentElement = makeRefPtr(document().documentElement());
    MediaQueryEvaluator evaluator { document().printing() ? "print" : "screen", document(), documentElement ? documentElement->computedStyle() : nullptr };
    for (auto& result : m_appearanceDependentMediaQueryResults) {
        LOG(MediaQueries, "HTMLPictureElement %p appearanceChangeAffectedPicture evaluating media queries", this);
        if (evaluator.evaluate(result.expression) != result.result)
            return true;
    }
    return false;
}

#if USE(SYSTEM_PREVIEW)
bool HTMLPictureElement::isSystemPreviewImage() const
{
    if (!RuntimeEnabledFeatures::sharedFeatures().systemPreviewEnabled())
        return false;

    const auto* parent = parentElement();
    if (!is<HTMLAnchorElement>(parent))
        return false;
    return downcast<HTMLAnchorElement>(parent)->isSystemPreviewLink();
}
#endif

}

