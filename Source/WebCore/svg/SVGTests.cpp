/*
 * Copyright (C) 2004, 2005, 2008 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005, 2006, 2007 Rob Buis <buis@kde.org>
 * Copyright (C) 2015 Apple Inc. All right reserved.
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

#include "DOMImplementation.h"
#include "HTMLNames.h"
#include "Language.h"
#include "SVGElement.h"
#include "SVGNames.h"
#include "SVGStringList.h"
#include <wtf/NeverDestroyed.h>

#if ENABLE(MATHML)
#include "MathMLNames.h"
#endif

namespace WebCore {

using namespace SVGNames;

SVGTests::SVGTests()
    : m_requiredFeatures(requiredFeaturesAttr)
    , m_requiredExtensions(requiredExtensionsAttr)
    , m_systemLanguage(systemLanguageAttr)
{
}

static SVGPropertyInfo createSVGTestPropertyInfo(const QualifiedName& attributeName, SVGPropertyInfo::SynchronizeProperty synchronizeFunction)
{
    return { AnimatedUnknown, PropertyIsReadWrite, attributeName, attributeName.localName(), synchronizeFunction, nullptr };
}

static SVGAttributeToPropertyMap createSVGTextAttributeToPropertyMap()
{
    typedef NeverDestroyed<const SVGPropertyInfo> Info;

    SVGAttributeToPropertyMap map;

    static Info requiredFeatures = createSVGTestPropertyInfo(requiredFeaturesAttr, SVGElement::synchronizeRequiredFeatures);
    map.addProperty(requiredFeatures.get());

    static Info requiredExtensions = createSVGTestPropertyInfo(requiredExtensionsAttr, SVGElement::synchronizeRequiredExtensions);
    map.addProperty(requiredExtensions.get());

    static Info systemLanguage = createSVGTestPropertyInfo(systemLanguageAttr, SVGElement::synchronizeSystemLanguage);
    map.addProperty(systemLanguage.get());

    return map;
}

const SVGAttributeToPropertyMap& SVGTests::attributeToPropertyMap()
{
    static NeverDestroyed<SVGAttributeToPropertyMap> map = createSVGTextAttributeToPropertyMap();
    return map;
}

bool SVGTests::hasExtension(const String& extension)
{
    // We recognize XHTML and MathML, as implemented in Gecko and suggested in the SVG Tiny recommendation (http://www.w3.org/TR/SVG11/struct.html#RequiredExtensionsAttribute).
#if ENABLE(MATHML)
    if (extension == MathMLNames::mathmlNamespaceURI)
        return true;
#endif
    return extension == HTMLNames::xhtmlNamespaceURI;
}

bool SVGTests::isValid() const
{
    for (auto& feature : m_requiredFeatures.value) {
        if (feature.isEmpty() || !DOMImplementation::hasFeature(feature, String()))
            return false;
    }
    for (auto& language : m_systemLanguage.value) {
        if (language != defaultLanguage().substring(0, 2))
            return false;
    }
    for (auto& extension : m_requiredExtensions.value) {
        if (!hasExtension(extension))
            return false;
    }
    return true;
}

void SVGTests::parseAttribute(const QualifiedName& attributeName, const AtomicString& value)
{
    if (attributeName == requiredFeaturesAttr)
        m_requiredFeatures.value.reset(value);
    if (attributeName == requiredExtensionsAttr)
        m_requiredExtensions.value.reset(value);
    if (attributeName == systemLanguageAttr)
        m_systemLanguage.value.reset(value);
}

bool SVGTests::isKnownAttribute(const QualifiedName& attributeName)
{
    return attributeName == requiredFeaturesAttr
        || attributeName == requiredExtensionsAttr
        || attributeName == systemLanguageAttr;
}

bool SVGTests::handleAttributeChange(SVGElement* targetElement, const QualifiedName& attributeName)
{
    ASSERT(targetElement);
    if (!isKnownAttribute(attributeName))
        return false;
    if (!targetElement->inDocument())
        return true;
    targetElement->setNeedsStyleRecalc(ReconstructRenderTree);
    return true;
}

void SVGTests::addSupportedAttributes(HashSet<QualifiedName>& supportedAttributes)
{
    supportedAttributes.add(requiredFeaturesAttr);
    supportedAttributes.add(requiredExtensionsAttr);
    supportedAttributes.add(systemLanguageAttr);
}

void SVGTests::synchronizeAttribute(SVGElement* contextElement, SVGSynchronizableAnimatedProperty<SVGStringList>& property, const QualifiedName& attributeName)
{
    ASSERT(contextElement);
    if (!property.shouldSynchronize)
        return;
    m_requiredFeatures.synchronize(contextElement, attributeName, property.value.valueAsString());
}

void SVGTests::synchronizeRequiredFeatures(SVGElement* contextElement)
{
    synchronizeAttribute(contextElement, m_requiredFeatures, requiredFeaturesAttr);
}

void SVGTests::synchronizeRequiredExtensions(SVGElement* contextElement)
{
    synchronizeAttribute(contextElement, m_requiredExtensions, requiredExtensionsAttr);
}

void SVGTests::synchronizeSystemLanguage(SVGElement* contextElement)
{
    synchronizeAttribute(contextElement, m_systemLanguage, systemLanguageAttr);
}

SVGStringList& SVGTests::requiredFeatures()
{
    m_requiredFeatures.shouldSynchronize = true;
    return m_requiredFeatures.value;
}

SVGStringList& SVGTests::requiredExtensions()
{
    m_requiredExtensions.shouldSynchronize = true;    
    return m_requiredExtensions.value;
}

SVGStringList& SVGTests::systemLanguage()
{
    m_systemLanguage.shouldSynchronize = true;
    return m_systemLanguage.value;
}

}
