/*
 * Copyright (C) 2004, 2005, 2008 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005, 2006, 2007 Rob Buis <buis@kde.org>
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

#include "Attribute.h"
#include "DOMImplementation.h"
#include "HTMLNames.h"
#include "Language.h"
#include "SVGElement.h"
#include "SVGNames.h"
#include "SVGStringList.h"

#if ENABLE(MATHML)
#include "MathMLNames.h"
#endif

namespace WebCore {

// Define custom non-animated property 'requiredFeatures'.
const SVGPropertyInfo* SVGTests::requiredFeaturesPropertyInfo()
{
    static const SVGPropertyInfo* s_propertyInfo = 0;
    if (!s_propertyInfo) {
        s_propertyInfo = new SVGPropertyInfo(AnimatedUnknown,
                                             PropertyIsReadWrite,
                                             SVGNames::requiredFeaturesAttr,
                                             SVGNames::requiredFeaturesAttr.localName(),
                                             &SVGElement::synchronizeRequiredFeatures,
                                             0);
    }
    return s_propertyInfo;
}

// Define custom non-animated property 'requiredExtensions'.
const SVGPropertyInfo* SVGTests::requiredExtensionsPropertyInfo()
{
    static const SVGPropertyInfo* s_propertyInfo = 0;
    if (!s_propertyInfo) {
        s_propertyInfo = new SVGPropertyInfo(AnimatedUnknown,
                                             PropertyIsReadWrite,
                                             SVGNames::requiredExtensionsAttr,
                                             SVGNames::requiredExtensionsAttr.localName(),
                                             &SVGElement::synchronizeRequiredExtensions,
                                             0);
    }
    return s_propertyInfo;
}

// Define custom non-animated property 'systemLanguage'.
const SVGPropertyInfo* SVGTests::systemLanguagePropertyInfo()
{
    static const SVGPropertyInfo* s_propertyInfo = 0;
    if (!s_propertyInfo) {
        s_propertyInfo = new SVGPropertyInfo(AnimatedUnknown,
                                             PropertyIsReadWrite,
                                             SVGNames::systemLanguageAttr,
                                             SVGNames::systemLanguageAttr.localName(),
                                             &SVGElement::synchronizeSystemLanguage,
                                             0);
    }
    return s_propertyInfo;
}

SVGTests::SVGTests()
    : m_requiredFeatures(SVGNames::requiredFeaturesAttr)
    , m_requiredExtensions(SVGNames::requiredExtensionsAttr)
    , m_systemLanguage(SVGNames::systemLanguageAttr)
{
}

SVGAttributeToPropertyMap& SVGTests::attributeToPropertyMap()
{
    DEFINE_STATIC_LOCAL(SVGAttributeToPropertyMap, map, ());
    if (!map.isEmpty())
        return map;
    map.addProperty(requiredFeaturesPropertyInfo());
    map.addProperty(requiredExtensionsPropertyInfo());
    map.addProperty(systemLanguagePropertyInfo());
    return map;
}

bool SVGTests::hasExtension(const String& extension) const
{
    // We recognize XHTML and MathML, as implemented in Gecko and suggested in the SVG Tiny recommendation (http://www.w3.org/TR/SVG11/struct.html#RequiredExtensionsAttribute).
#if ENABLE(MATHML)
    return extension == HTMLNames::xhtmlNamespaceURI || extension == MathMLNames::mathmlNamespaceURI;
#else
    return extension == HTMLNames::xhtmlNamespaceURI;
#endif
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

bool SVGTests::parseAttribute(const QualifiedName& name, const AtomicString& value)
{
    if (name == SVGNames::requiredFeaturesAttr) {
        m_requiredFeatures.value.reset(value);
        return true;
    }
    if (name == SVGNames::requiredExtensionsAttr) {
        m_requiredExtensions.value.reset(value);
        return true;
    }
    if (name == SVGNames::systemLanguageAttr) {
        m_systemLanguage.value.reset(value);
        return true;
    }
    
    return false;
}

bool SVGTests::isKnownAttribute(const QualifiedName& attrName)
{
    return attrName == SVGNames::requiredFeaturesAttr
        || attrName == SVGNames::requiredExtensionsAttr
        || attrName == SVGNames::systemLanguageAttr;
}

bool SVGTests::handleAttributeChange(SVGElement* targetElement, const QualifiedName& attrName)
{
    ASSERT(targetElement);
    if (!isKnownAttribute(attrName))
        return false;
    if (!targetElement->inDocument())
        return true;

    targetElement->setNeedsStyleRecalc(ReconstructRenderTree);

    return true;
}

void SVGTests::addSupportedAttributes(HashSet<QualifiedName>& supportedAttributes)
{
    supportedAttributes.add(SVGNames::requiredFeaturesAttr);
    supportedAttributes.add(SVGNames::requiredExtensionsAttr);
    supportedAttributes.add(SVGNames::systemLanguageAttr);
}

void SVGTests::synchronizeRequiredFeatures(SVGElement* contextElement)
{
    ASSERT(contextElement);
    if (!m_requiredFeatures.shouldSynchronize)
        return;
    AtomicString value(m_requiredFeatures.value.valueAsString());
    m_requiredFeatures.synchronize(contextElement, requiredFeaturesPropertyInfo()->attributeName, value);
}

void SVGTests::synchronizeRequiredExtensions(SVGElement* contextElement)
{
    ASSERT(contextElement);
    if (!m_requiredExtensions.shouldSynchronize)
        return;
    AtomicString value(m_requiredExtensions.value.valueAsString());
    m_requiredExtensions.synchronize(contextElement, requiredExtensionsPropertyInfo()->attributeName, value);
}

void SVGTests::synchronizeSystemLanguage(SVGElement* contextElement)
{
    ASSERT(contextElement);
    if (!m_systemLanguage.shouldSynchronize)
        return;
    AtomicString value(m_systemLanguage.value.valueAsString());
    m_systemLanguage.synchronize(contextElement, systemLanguagePropertyInfo()->attributeName, value);
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
