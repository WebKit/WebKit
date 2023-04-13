/*
 * Copyright (C) 2004, 2005, 2008 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005, 2006, 2007 Rob Buis <buis@kde.org>
 * Copyright (C) 2015-2021 Apple Inc. All right reserved.
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
#include "SVGElement.h"
#include "SVGNames.h"
#include "SVGStringList.h"
#include <wtf/Language.h>
#include <wtf/SortedArrayMap.h>

#if ENABLE(MATHML)
#include "MathMLNames.h"
#endif

namespace WebCore {

constexpr ComparableLettersLiteral supportedSVGFeatureArray[] = {
    "http://www.w3.org/tr/svg11/feature#animation",
    "http://www.w3.org/tr/svg11/feature#basegraphicsattribute",
    "http://www.w3.org/tr/svg11/feature#basicclip",
    "http://www.w3.org/tr/svg11/feature#basicfilter",
    "http://www.w3.org/tr/svg11/feature#basicfont",
    "http://www.w3.org/tr/svg11/feature#basicpaintattribute",
    "http://www.w3.org/tr/svg11/feature#basicstructure",
    "http://www.w3.org/tr/svg11/feature#basictext",
    "http://www.w3.org/tr/svg11/feature#clip",
    "http://www.w3.org/tr/svg11/feature#conditionalprocessing",
    "http://www.w3.org/tr/svg11/feature#containerattribute",
    "http://www.w3.org/tr/svg11/feature#coreattribute",
    "http://www.w3.org/tr/svg11/feature#cursor",
    "http://www.w3.org/tr/svg11/feature#documenteventsattribute",
    "http://www.w3.org/tr/svg11/feature#extensibility",
    "http://www.w3.org/tr/svg11/feature#externalresourcesrequired",
    "http://www.w3.org/tr/svg11/feature#filter",
    "http://www.w3.org/tr/svg11/feature#font",
    "http://www.w3.org/tr/svg11/feature#gradient",
    "http://www.w3.org/tr/svg11/feature#graphicaleventsattribute",
    "http://www.w3.org/tr/svg11/feature#graphicsattribute",
    "http://www.w3.org/tr/svg11/feature#hyperlinking",
    "http://www.w3.org/tr/svg11/feature#image",
    "http://www.w3.org/tr/svg11/feature#marker",
    "http://www.w3.org/tr/svg11/feature#mask",
    "http://www.w3.org/tr/svg11/feature#opacityattribute",
    "http://www.w3.org/tr/svg11/feature#paintattribute",
    "http://www.w3.org/tr/svg11/feature#pattern",
    "http://www.w3.org/tr/svg11/feature#script",
    "http://www.w3.org/tr/svg11/feature#shape",
    "http://www.w3.org/tr/svg11/feature#structure",
    "http://www.w3.org/tr/svg11/feature#style",
    "http://www.w3.org/tr/svg11/feature#svg",
    "http://www.w3.org/tr/svg11/feature#svg-animation",
    "http://www.w3.org/tr/svg11/feature#svg-static",
    "http://www.w3.org/tr/svg11/feature#svgdom",
    "http://www.w3.org/tr/svg11/feature#svgdom-animation",
    "http://www.w3.org/tr/svg11/feature#svgdom-static",
    "http://www.w3.org/tr/svg11/feature#text",
    "http://www.w3.org/tr/svg11/feature#view",
    "http://www.w3.org/tr/svg11/feature#viewportattribute",
    "http://www.w3.org/tr/svg11/feature#xlinkattribute",
    "org.w3c.dom",
    "org.w3c.dom.svg",
    "org.w3c.dom.svg.static",
    "org.w3c.svg",
    "org.w3c.svg.static",
};

constexpr SortedArraySet supportedSVGFeatureSet { supportedSVGFeatureArray };

SVGConditionalProcessingAttributes::SVGConditionalProcessingAttributes(SVGElement& contextElement)
    : m_requiredFeatures(SVGStringList::create(&contextElement))
    , m_requiredExtensions(SVGStringList::create(&contextElement))
    , m_systemLanguage(SVGStringList::create(&contextElement))
{
    static std::once_flag onceFlag;
    std::call_once(onceFlag, [] {
        SVGTests::PropertyRegistry::registerConditionalProcessingAttributeProperty<SVGNames::requiredFeaturesAttr, &SVGConditionalProcessingAttributes::m_requiredFeatures>();
        SVGTests::PropertyRegistry::registerConditionalProcessingAttributeProperty<SVGNames::requiredExtensionsAttr, &SVGConditionalProcessingAttributes::m_requiredExtensions>();
        SVGTests::PropertyRegistry::registerConditionalProcessingAttributeProperty<SVGNames::systemLanguageAttr, &SVGConditionalProcessingAttributes::m_systemLanguage>();
    });
}

SVGTests::SVGTests(SVGElement* contextElement)
    : m_contextElement(*contextElement)
{
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
    auto attributes = conditionalProcessingAttributesIfExists();
    if (!attributes)
        return true;

    for (auto& feature : attributes->requiredFeatures().items()) {
        if (feature.isEmpty() || !supportedSVGFeatureSet.contains(feature))
            return false;
    }
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

void SVGTests::parseAttribute(const QualifiedName& attributeName, const AtomString& value)
{
    if (attributeName == SVGNames::requiredFeaturesAttr)
        requiredFeatures().reset(value);
    if (attributeName == SVGNames::requiredExtensionsAttr)
        requiredExtensions().reset(value);
    if (attributeName == SVGNames::systemLanguageAttr)
        systemLanguage().reset(value);
}

void SVGTests::svgAttributeChanged(const QualifiedName& attrName)
{
    if (!PropertyRegistry::isKnownAttribute(attrName))
        return;

    if (!m_contextElement.isConnected())
        return;
    m_contextElement.invalidateStyleAndRenderersForSubtree();
}

void SVGTests::addSupportedAttributes(MemoryCompactLookupOnlyRobinHoodHashSet<QualifiedName>& supportedAttributes)
{
    supportedAttributes.add(SVGNames::requiredFeaturesAttr);
    supportedAttributes.add(SVGNames::requiredExtensionsAttr);
    supportedAttributes.add(SVGNames::systemLanguageAttr);
}

bool SVGTests::hasFeatureForLegacyBindings(const String& feature, const String& version)
{
    // FIXME: This function is here only to be exposed in the Objective-C and GObject bindings for both Node and DOMImplementation.
    // It's likely that we can just remove this and instead have the bindings return true unconditionally.
    // This is what the DOMImplementation function now does in JavaScript as is now suggested in the DOM specification.
    // The behavior implemented below is quirky, but preserves what WebKit has done for at least the last few years.

    bool hasSVG10FeaturePrefix = startsWithLettersIgnoringASCIICase(feature, "org.w3c.dom.svg"_s) || startsWithLettersIgnoringASCIICase(feature, "org.w3c.svg"_s);
    bool hasSVG11FeaturePrefix = startsWithLettersIgnoringASCIICase(feature, "http://www.w3.org/tr/svg"_s);

    // We don't even try to handle feature names that don't look like the SVG ones, so just return true for all of those.
    if (!(hasSVG10FeaturePrefix || hasSVG11FeaturePrefix))
        return true;

    // If the version number matches the style of the feature name, then use the set to see if the feature is supported.
    if (version.isEmpty() || (hasSVG10FeaturePrefix && version == "1.0"_s) || (hasSVG11FeaturePrefix && version == "1.1"_s))
        return supportedSVGFeatureSet.contains(feature);

    return false;
}

SVGConditionalProcessingAttributes& SVGTests::conditionalProcessingAttributes()
{
    return m_contextElement.conditionalProcessingAttributes();
}

SVGConditionalProcessingAttributes* SVGTests::conditionalProcessingAttributesIfExists() const
{
    return m_contextElement.conditionalProcessingAttributesIfExists();
}

}
