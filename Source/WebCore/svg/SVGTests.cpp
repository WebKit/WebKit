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

#include "EventTarget.h"
#include "HTMLNames.h"
#include "NodeName.h"
#include "SVGElement.h"
#include "SVGNames.h"
#include "SVGPropertyOwnerRegistry.h"
#include "SVGStringList.h"
#include <wtf/Language.h>
#include <wtf/SortedArrayMap.h>
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

bool SVGTests::isValid() const
{
    auto attributes = conditionalProcessingAttributesIfExists();
    if (!attributes)
        return true;

    String defaultLanguage = WTF::defaultLanguage();
    auto genericDefaultLanguage = StringView(defaultLanguage).left(2);
    for (auto& language : attributes->systemLanguage().items()) {
        if (language != genericDefaultLanguage)
            return false;
    }
    for (auto& extension : attributes->requiredExtensions().items()) {
        if (!hasExtension(extension))
            return false;
    }
    return true;
}

Ref<SVGStringList> SVGTests::protectedRequiredExtensions()
{
    return requiredExtensions();
}

Ref<SVGStringList> SVGTests::protectedSystemLanguage()
{
    return systemLanguage();
}

void SVGTests::parseAttribute(const QualifiedName& attributeName, const AtomString& value)
{
    switch (attributeName.nodeName()) {
    case AttributeNames::requiredExtensionsAttr:
        protectedRequiredExtensions()->reset(value);
        break;
    case AttributeNames::systemLanguageAttr:
        protectedSystemLanguage()->reset(value);
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

Ref<SVGElement> SVGTests::protectedContextElement() const
{
    return m_contextElement.get();
}

SVGConditionalProcessingAttributes& SVGTests::conditionalProcessingAttributes()
{
    Ref<SVGElement> contextElement = m_contextElement.get();
    return contextElement->conditionalProcessingAttributes();
}

SVGConditionalProcessingAttributes* SVGTests::conditionalProcessingAttributesIfExists() const
{
    return protectedContextElement()->conditionalProcessingAttributesIfExists();
}

} // namespace WebCore
