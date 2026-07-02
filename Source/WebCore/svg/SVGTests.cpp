/*
 * Copyright (C) 2004, 2005, 2008 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005, 2006, 2007 Rob Buis <buis@kde.org>
 * Copyright (C) 2015-2024 Apple Inc. All right reserved.
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
#include "SVGTests.h"

#include "HTMLNames.h"
#include "NodeName.h"
#include "SVGElement.h"
#include "SVGNames.h"
#include "SVGPropertyOwnerRegistry.h"
#include "SVGStringList.h"
#include <wtf/Language.h>
#include <wtf/TZoneMallocInlines.h>

#if ENABLE(MATHML)
#include "MathMLNames.h"
#endif

namespace WebCore {

WTF_MAKE_TZONE_ALLOCATED_IMPL(SVGConditionalProcessingAttributes);

SVGConditionalProcessingAttributes::SVGConditionalProcessingAttributes(SVGElement& contextElement)
    : m_requiredExtensions(SVGStringList::create(&contextElement))
    , m_systemLanguage(SVGStringList::create(&contextElement))
{
    static bool didRegistration = false;
    if (!didRegistration) [[unlikely]] {
        didRegistration = true;
        SVGTests::PropertyRegistry::registerConditionalProcessingAttributeProperty<SVGNames::requiredExtensionsAttr, &SVGConditionalProcessingAttributes::m_requiredExtensions>();
        SVGTests::PropertyRegistry::registerConditionalProcessingAttributeProperty<SVGNames::systemLanguageAttr, &SVGConditionalProcessingAttributes::m_systemLanguage>();
    }
}

SVGTests::SVGTests(SVGElement* contextElement)
    : m_contextElement(*contextElement)
{
}

bool SVGTests::hasExtension(const String& extension)
{
    // We recognize XHTML and MathML, as implemented in Gecko and suggested in the SVG Tiny
    // recommendation (http://www.w3.org/TR/SVG11/struct.html#RequiredExtensionsAttribute).
#if ENABLE(MATHML)
    if (extension == MathMLNames::mathmlNamespaceURI)
        return true;
#endif
    return extension == HTMLNames::xhtmlNamespaceURI;
}

// FIXME: RFC 4647 Basic Filtering with parent language tags can cause cross-script
// matches (e.g., "zh" matching "zh-Hant" when user prefers "zh-Hans"). Consider
// Extended Filtering (RFC 4647 3.3.2) for script-aware matching.
static bool matchesLanguageTag(StringView range, StringView tag)
{
    if (range.isEmpty())
        return false;
    if (range.length() == tag.length())
        return equalIgnoringASCIICase(range, tag);
    return range.length() < tag.length()
        && tag[range.length()] == '-'
        && equalIgnoringASCIICase(range, tag.left(range.length()));
}

// SVG2 5.6.5 implementation note: try the full user language tag, then iteratively
// strip subtags to synthesize parent language tags (e.g. "zh-Hans-CN" → "zh-Hans" → "zh").
static bool userLanguageMatchesAnyListedTag(StringView userLanguage, const Vector<String>& listedTags)
{
    for (StringView range = userLanguage; !range.isEmpty(); ) {
        for (auto& listedTag : listedTags) {
            if (matchesLanguageTag(range, listedTag))
                return true;
        }
        auto hyphenIndex = range.reverseFind('-');
        if (hyphenIndex == notFound)
            return false;
        range = range.left(hyphenIndex);
    }
    return false;
}

bool SVGTests::isValid() const
{
    auto attributes = conditionalProcessingAttributesIfExists();
    if (!attributes)
        return true;

    auto& systemLanguageList = attributes->systemLanguage().items();
    if (!systemLanguageList.isEmpty()) {
        // Match against the same value navigator.language / navigator.languages exposes.
        if (!userLanguageMatchesAnyListedTag(defaultLanguage(), systemLanguageList))
            return false;
    }

    for (auto& extension : attributes->requiredExtensions().items()) {
        if (!hasExtension(extension))
            return false;
    }
    return true;
}

void SVGTests::parseAttribute(const QualifiedName& attributeName, const AtomString& value)
{
    switch (attributeName.nodeName()) {
    case AttributeNames::requiredExtensionsAttr:
        protect(requiredExtensions())->setFromSpaceSeparatedTokens(value);
        break;
    case AttributeNames::systemLanguageAttr:
        protect(systemLanguage())->setFromCommaSeparatedTokens(value);
        break;
    default:
        break;
    }
}

void SVGTests::svgAttributeChanged(const QualifiedName& attrName)
{
    if (!PropertyRegistry::isKnownAttribute(attrName))
        return;

    Ref contextElement = m_contextElement.get();
    if (!contextElement->isConnected())
        return;
    contextElement->invalidateStyleAndRenderersForSubtree();
}

void SVGTests::addSupportedAttributes(MemoryCompactLookupOnlyRobinHoodHashSet<QualifiedName>& supportedAttributes)
{
    supportedAttributes.add(SVGNames::requiredExtensionsAttr);
    supportedAttributes.add(SVGNames::systemLanguageAttr);
}

SVGConditionalProcessingAttributes& SVGTests::conditionalProcessingAttributes()
{
    Ref contextElement = m_contextElement;
    return contextElement->conditionalProcessingAttributes();
}

SVGConditionalProcessingAttributes* SVGTests::conditionalProcessingAttributesIfExists() const
{
    return m_contextElement->conditionalProcessingAttributesIfExists();
}

} // namespace WebCore
