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
#include "ImageLoader.h"
#include "Logging.h"
#include "Settings.h"
#include <wtf/IsoMallocInlines.h>

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(HTMLPictureElement);

HTMLPictureElement::HTMLPictureElement(const QualifiedName& tagName, Document& document)
    : HTMLElement(tagName, document)
{
}

HTMLPictureElement::~HTMLPictureElement()
{
}

void HTMLPictureElement::didMoveToNewDocument(Document& oldDocument, Document& newDocument)
{
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
        element.selectImageSource(RelevantMutation::Yes);
}

void HTMLPictureElement::sourceDimensionAttributesChanged(const HTMLSourceElement& sourceElement)
{
    for (auto& element : childrenOfType<HTMLImageElement>(*this)) {
        if (&sourceElement == element.sourceElement())
            element.invalidateAttributeMapping();
    }
}

#if USE(SYSTEM_PREVIEW)
bool HTMLPictureElement::isSystemPreviewImage()
{
    if (!document().settings().systemPreviewEnabled())
        return false;

    auto* parent = parentElement();
    if (!is<HTMLAnchorElement>(parent))
        return false;
    return downcast<HTMLAnchorElement>(parent)->isSystemPreviewLink();
}
#endif

}

