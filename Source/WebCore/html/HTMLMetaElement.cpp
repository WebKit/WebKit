/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2001 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2003-2023 Apple Inc. All rights reserved.
 * Copyright (C) 2013 Google Inc. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include "config.h"
#include "HTMLMetaElement.h"

#include "Attribute.h"
#include "CSSParser.h"
#include "Color.h"
#include "Document.h"
#include "DocumentInlines.h"
#include "ElementInlines.h"
#include "HTMLHeadElement.h"
#include "HTMLNames.h"
#include "HTMLParserIdioms.h"
#include "LocalFrame.h"
#include "LocalFrameView.h"
#include "MediaQueryEvaluator.h"
#include "MediaQueryParser.h"
#include "MediaQueryParserContext.h"
#include "NodeName.h"
#include "Quirks.h"
#include "RenderStyle.h"
#include "Settings.h"
#include "StyleResolveForDocument.h"
#include <wtf/TZoneMallocInlines.h>

namespace WebCore {

WTF_MAKE_TZONE_OR_ISO_ALLOCATED_IMPL(HTMLMetaElement);

using namespace HTMLNames;

inline HTMLMetaElement::HTMLMetaElement(const QualifiedName& tagName, Document& document)
    : HTMLElement(tagName, document)
{
    ASSERT(hasTagName(metaTag));
}

Ref<HTMLMetaElement> HTMLMetaElement::create(Document& document)
{
    return adoptRef(*new HTMLMetaElement(metaTag, document));
}

Ref<HTMLMetaElement> HTMLMetaElement::create(const QualifiedName& tagName, Document& document)
{
    return adoptRef(*new HTMLMetaElement(tagName, document));
}

#if ENABLE(DARK_MODE_CSS)
static bool isNameColorScheme(const AtomString& nameValue)
{
    return equalLettersIgnoringASCIICase(nameValue, "color-scheme"_s) || equalLettersIgnoringASCIICase(nameValue, "supported-color-schemes"_s);
}
#endif

bool HTMLMetaElement::mediaAttributeMatches()
{
    auto& document = this->document();

    if (!m_mediaQueryList) {
        auto mediaText = attributeWithoutSynchronization(mediaAttr).convertToASCIILowercase();
        m_mediaQueryList = MQ::MediaQueryParser::parse(mediaText, { document });
    }

    std::optional<RenderStyle> documentStyle;
    if (document.hasLivingRenderTree())
        documentStyle = Style::resolveForDocument(document);

    AtomString mediaType;
    if (auto* frame = document.frame()) {
        if (auto* frameView = frame->view())
            mediaType = frameView->mediaType();
    }

    auto evaluator = MQ::MediaQueryEvaluator { mediaType, document, documentStyle ? &*documentStyle : nullptr };
    return evaluator.evaluate(*m_mediaQueryList);
}

const Color& HTMLMetaElement::contentColor()
{
    if (!m_contentColor)
        m_contentColor = CSSParser::parseColorWithoutContext(content());
    return *m_contentColor;
}

void HTMLMetaElement::attributeChanged(const QualifiedName& name, const AtomString& oldValue, const AtomString& newValue, AttributeModificationReason attributeModificationReason)
{
    HTMLElement::attributeChanged(name, oldValue, newValue, attributeModificationReason);

    switch (name.nodeName()) {
    case AttributeNames::nameAttr:
        process(oldValue);
        if (isInDocumentTree()) {
            if (equalLettersIgnoringASCIICase(oldValue, "theme-color"_s) && !equalLettersIgnoringASCIICase(newValue, "theme-color"_s))
                document().metaElementThemeColorChanged(*this);
        }
        break;
    case AttributeNames::contentAttr:
        m_contentColor = std::nullopt;
        process();
        break;
    case AttributeNames::http_equivAttr:
        process();
        break;
    case AttributeNames::mediaAttr:
        m_mediaQueryList = { };
        process();
        break;
    default:
        break;
    }
}

Node::InsertedIntoAncestorResult HTMLMetaElement::insertedIntoAncestor(InsertionType insertionType, ContainerNode& parentOfInsertedTree)
{
    HTMLElement::insertedIntoAncestor(insertionType, parentOfInsertedTree);
    if (insertionType.connectedToDocument)
        return InsertedIntoAncestorResult::NeedsPostInsertionCallback;
    return InsertedIntoAncestorResult::Done;
}

void HTMLMetaElement::didFinishInsertingNode()
{
    process();
}

void HTMLMetaElement::removedFromAncestor(RemovalType removalType, ContainerNode& oldParentOfRemovedTree)
{
    HTMLElement::removedFromAncestor(removalType, oldParentOfRemovedTree);

    if (removalType.disconnectedFromDocument && equalLettersIgnoringASCIICase(name(), "theme-color"_s))
        oldParentOfRemovedTree.document().metaElementThemeColorChanged(*this);
#if ENABLE(DARK_MODE_CSS)
    else if (removalType.disconnectedFromDocument && isNameColorScheme(name()))
        oldParentOfRemovedTree.document().metaElementColorSchemeChanged();
#endif
}

void HTMLMetaElement::process(const AtomString& oldValue)
{
    // Changing a meta tag while it's not in the document tree shouldn't have any effect on the document.
    if (!isInDocumentTree())
        return;

    const AtomString& nameValue = attributeWithoutSynchronization(nameAttr);
#if ENABLE(DARK_MODE_CSS)
    if (isNameColorScheme(nameValue) || (!oldValue.isNull() && isNameColorScheme(oldValue)))
        document().metaElementColorSchemeChanged();
#else
    UNUSED_PARAM(oldValue);
#endif

    // https://html.spec.whatwg.org/multipage/semantics.html#the-meta-element
    // All below situations require a content attribute (which can be the empty string).
    const AtomString& contentValue = attributeWithoutSynchronization(contentAttr);
    if (contentValue.isNull())
        return;
    
    const AtomString& httpEquivValue = attributeWithoutSynchronization(http_equivAttr);
    // Get the document to process the tag, but only if we're actually part of DOM
    // tree (changing a meta tag while it's not in the tree shouldn't have any effect
    // on the document)
    if (!httpEquivValue.isNull())
        document().processMetaHttpEquiv(httpEquivValue, contentValue, isDescendantOf(document().head()));
    
    if (nameValue.isNull())
        return;

    if (equalLettersIgnoringASCIICase(nameValue, "viewport"_s))
        document().processViewport(contentValue, ViewportArguments::Type::ViewportMeta);
    else if (document().settings().disabledAdaptationsMetaTagEnabled() && equalLettersIgnoringASCIICase(nameValue, "disabled-adaptations"_s))
        document().processDisabledAdaptations(contentValue);
    else if (equalLettersIgnoringASCIICase(nameValue, "theme-color"_s))
        document().metaElementThemeColorChanged(*this);
#if PLATFORM(IOS_FAMILY)
    else if (equalLettersIgnoringASCIICase(nameValue, "format-detection"_s))
        document().processFormatDetection(contentValue);
    else if (equalLettersIgnoringASCIICase(nameValue, "apple-mobile-web-app-orientations"_s))
        document().processWebAppOrientations();
#endif
    else if (equalLettersIgnoringASCIICase(nameValue, "referrer"_s))
        document().processReferrerPolicy(contentValue, ReferrerPolicySource::MetaTag);
    else if (equalLettersIgnoringASCIICase(nameValue, "confluence-request-time"_s))
        document().quirks().setNeedsToCopyUserSelectNoneQuirk();
}

const AtomString& HTMLMetaElement::content() const
{
    return attributeWithoutSynchronization(contentAttr);
}

const AtomString& HTMLMetaElement::httpEquiv() const
{
    return attributeWithoutSynchronization(http_equivAttr);
}

const AtomString& HTMLMetaElement::name() const
{
    return getNameAttribute();
}

}
