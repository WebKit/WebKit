/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2001 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2003, 2010 Apple Inc. All rights reserved.
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
#include "Color.h"
#include "Document.h"
#include "ElementInlines.h"
#include "Frame.h"
#include "FrameView.h"
#include "HTMLHeadElement.h"
#include "HTMLNames.h"
#include "HTMLParserIdioms.h"
#include "MediaQueryEvaluator.h"
#include "MediaQueryParser.h"
#include "MediaQueryParserContext.h"
#include "RenderStyle.h"
#include "Settings.h"
#include "StyleResolveForDocument.h"
#include <wtf/IsoMallocInlines.h>

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(HTMLMetaElement);

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
            mediaType = AtomString(frameView->mediaType());
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

void HTMLMetaElement::attributeChanged(const QualifiedName& name, const AtomString& oldValue, const AtomString& newValue, AttributeModificationReason reason)
{
    HTMLElement::attributeChanged(name, oldValue, newValue, reason);

    if (!isInDocumentTree())
        return;

    if (name == nameAttr) {
        if (equalLettersIgnoringASCIICase(oldValue, "theme-color"_s) && !equalLettersIgnoringASCIICase(newValue, "theme-color"_s))
            document().metaElementThemeColorChanged(*this);
        return;
    }
}

void HTMLMetaElement::parseAttribute(const QualifiedName& name, const AtomString& value)
{
    if (name == nameAttr) {
        process();
        return;
    }

    if (name == contentAttr) {
        m_contentColor = std::nullopt;
        process();
        return;
    }

    if (name == http_equivAttr) {
        process();
        return;
    }

    if (name == mediaAttr) {
        m_mediaQueryList = { };
        process();
        return;
    }

    HTMLElement::parseAttribute(name, value);
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
}

void HTMLMetaElement::process()
{
    // Changing a meta tag while it's not in the document tree shouldn't have any effect on the document.
    if (!isInDocumentTree())
        return;

    const AtomString& contentValue = attributeWithoutSynchronization(contentAttr);
    if (contentValue.isNull())
        return;

    if (equalLettersIgnoringASCIICase(name(), "viewport"_s))
        document().processViewport(contentValue, ViewportArguments::ViewportMeta);
    else if (document().settings().disabledAdaptationsMetaTagEnabled() && equalLettersIgnoringASCIICase(name(), "disabled-adaptations"_s))
        document().processDisabledAdaptations(contentValue);
#if ENABLE(DARK_MODE_CSS)
    else if (equalLettersIgnoringASCIICase(name(), "color-scheme"_s) || equalLettersIgnoringASCIICase(name(), "supported-color-schemes"_s))
        document().processColorScheme(contentValue);
#endif
    else if (equalLettersIgnoringASCIICase(name(), "theme-color"_s))
        document().metaElementThemeColorChanged(*this);
#if PLATFORM(IOS_FAMILY)
    else if (equalLettersIgnoringASCIICase(name(), "format-detection"_s))
        document().processFormatDetection(contentValue);
    else if (equalLettersIgnoringASCIICase(name(), "apple-mobile-web-app-orientations"_s))
        document().processWebAppOrientations();
#endif
    else if (equalLettersIgnoringASCIICase(name(), "referrer"_s))
        document().processReferrerPolicy(contentValue, ReferrerPolicySource::MetaTag);

    const AtomString& httpEquivValue = attributeWithoutSynchronization(http_equivAttr);
    if (!httpEquivValue.isNull())
        document().processMetaHttpEquiv(httpEquivValue, contentValue, isDescendantOf(document().head()));
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
