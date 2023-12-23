/*
 * Copyright (C) 2013-2023 Apple Inc.  All rights reserved.
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
#include "WebVTTElement.h"

#if ENABLE(VIDEO)

#include "ElementInlines.h"
#include "HTMLSpanElement.h"
#include "RenderRuby.h"
#include "RenderRubyText.h"
#include "RenderTreePosition.h"
#include "RubyElement.h"
#include "RubyTextElement.h"
#include "TextTrack.h"
#include <wtf/IsoMallocInlines.h>

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(WebVTTElement);
WTF_MAKE_ISO_ALLOCATED_IMPL(WebVTTRubyElement);
WTF_MAKE_ISO_ALLOCATED_IMPL(WebVTTRubyTextElement);

static const QualifiedName& nodeTypeToTagName(WebVTTNodeType nodeType)
{
    static NeverDestroyed<QualifiedName> cTag(nullAtom(), "c"_s, nullAtom());
    static NeverDestroyed<QualifiedName> vTag(nullAtom(), "v"_s, nullAtom());
    static NeverDestroyed<QualifiedName> langTag(nullAtom(), "lang"_s, nullAtom());
    static NeverDestroyed<QualifiedName> bTag(nullAtom(), "b"_s, nullAtom());
    static NeverDestroyed<QualifiedName> uTag(nullAtom(), "u"_s, nullAtom());
    static NeverDestroyed<QualifiedName> iTag(nullAtom(), "i"_s, nullAtom());
    static NeverDestroyed<QualifiedName> rubyTag(nullAtom(), "ruby"_s, nullAtom());
    static NeverDestroyed<QualifiedName> rtTag(nullAtom(), "rt"_s, nullAtom());
    switch (nodeType) {
    case WebVTTNodeTypeClass:
        return cTag;
    case WebVTTNodeTypeItalic:
        return iTag;
    case WebVTTNodeTypeLanguage:
        return langTag;
    case WebVTTNodeTypeBold:
        return bTag;
    case WebVTTNodeTypeUnderline:
        return uTag;
    case WebVTTNodeTypeRuby:
        return rubyTag;
    case WebVTTNodeTypeRubyText:
        return rtTag;
    case WebVTTNodeTypeVoice:
        return vTag;
    case WebVTTNodeTypeNone:
    default:
        ASSERT_NOT_REACHED();
        return cTag; // Make the compiler happy.
    }
}

WebVTTElement::WebVTTElement(WebVTTNodeType nodeType, AtomString language, Document& document)
    : WebVTTElementImpl(nodeType, language)
    , Element(nodeTypeToTagName(nodeType), document, { })
{
}

Ref<Element> WebVTTElementImpl::create(WebVTTNodeType nodeType, AtomString language, Document& document)
{
    switch (nodeType) {
    default:
    case WebVTTNodeTypeNone:
    case WebVTTNodeTypeClass:
    case WebVTTNodeTypeItalic:
    case WebVTTNodeTypeLanguage:
    case WebVTTNodeTypeBold:
    case WebVTTNodeTypeUnderline:
    case WebVTTNodeTypeVoice:
        return adoptRef(*new WebVTTElement(nodeType, language, document));
    case WebVTTNodeTypeRuby:
        return adoptRef(*new WebVTTRubyElement(language, document));
    case WebVTTNodeTypeRubyText:
        return adoptRef(*new WebVTTRubyTextElement(language, document));
    }
}

Ref<Element> WebVTTElementImpl::cloneElementWithoutAttributesAndChildren(Document& targetDocument)
{
    return create(static_cast<WebVTTNodeType>(m_webVTTNodeType), m_language, targetDocument);
}

Ref<HTMLElement> WebVTTElementImpl::createEquivalentHTMLElement(Document& document)
{
    RefPtr<HTMLElement> htmlElement;

    switch (m_webVTTNodeType) {
    case WebVTTNodeTypeClass:
    case WebVTTNodeTypeLanguage:
    case WebVTTNodeTypeVoice:
        htmlElement = HTMLSpanElement::create(document);
        htmlElement->setAttributeWithoutSynchronization(HTMLNames::titleAttr, toElement().attributeWithoutSynchronization(voiceAttributeName()));
        htmlElement->setAttributeWithoutSynchronization(HTMLNames::langAttr, toElement().attributeWithoutSynchronization(langAttributeName()));
        break;
    case WebVTTNodeTypeItalic:
        htmlElement = HTMLElement::create(HTMLNames::iTag, document);
        break;
    case WebVTTNodeTypeBold:
        htmlElement = HTMLElement::create(HTMLNames::bTag, document);
        break;
    case WebVTTNodeTypeUnderline:
        htmlElement = HTMLElement::create(HTMLNames::uTag, document);
        break;
    case WebVTTNodeTypeRuby:
        htmlElement = RubyElement::create(document);
        break;
    case WebVTTNodeTypeRubyText:
        htmlElement = RubyTextElement::create(document);
        break;
    }

    ASSERT(htmlElement);
    if (htmlElement)
        htmlElement->setAttributeWithoutSynchronization(HTMLNames::classAttr, toElement().attributeWithoutSynchronization(HTMLNames::classAttr));
    return htmlElement.releaseNonNull();
}

} // namespace WebCore

#endif
