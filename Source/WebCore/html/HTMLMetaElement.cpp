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
#include "Document.h"
#include "HTMLHeadElement.h"
#include "HTMLNames.h"
#include "RuntimeEnabledFeatures.h"
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

void HTMLMetaElement::parseAttribute(const QualifiedName& name, const AtomString& value)
{
    if (name == http_equivAttr)
        process();
    else if (name == contentAttr)
        process();
    else if (name == nameAttr) {
        // Do nothing
    } else
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

void HTMLMetaElement::process()
{
    // Changing a meta tag while it's not in the tree shouldn't have any effect on the document.
    if (!isConnected())
        return;

    const AtomString& contentValue = attributeWithoutSynchronization(contentAttr);
    if (contentValue.isNull())
        return;

    if (equalLettersIgnoringASCIICase(name(), "viewport"))
        document().processViewport(contentValue, ViewportArguments::ViewportMeta);
    else if (RuntimeEnabledFeatures::sharedFeatures().disabledAdaptationsMetaTagEnabled() && equalLettersIgnoringASCIICase(name(), "disabled-adaptations"))
        document().processDisabledAdaptations(contentValue);
#if ENABLE(DARK_MODE_CSS)
    else if (equalLettersIgnoringASCIICase(name(), "color-scheme") || equalLettersIgnoringASCIICase(name(), "supported-color-schemes"))
        document().processColorScheme(contentValue);
#endif
#if PLATFORM(IOS_FAMILY)
    else if (equalLettersIgnoringASCIICase(name(), "format-detection"))
        document().processFormatDetection(contentValue);
    else if (equalLettersIgnoringASCIICase(name(), "apple-mobile-web-app-orientations"))
        document().processWebAppOrientations();
#endif
    else if (equalLettersIgnoringASCIICase(name(), "referrer"))
        document().processReferrerPolicy(contentValue, ReferrerPolicySource::MetaTag);

    const AtomString& httpEquivValue = attributeWithoutSynchronization(http_equivAttr);
    if (!httpEquivValue.isNull())
        document().processHttpEquiv(httpEquivValue, contentValue, isDescendantOf(document().head()));
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
