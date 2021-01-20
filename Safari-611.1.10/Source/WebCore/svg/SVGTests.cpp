/*
 * Copyright (C) 2004, 2005, 2008 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005, 2006, 2007 Rob Buis <buis@kde.org>
 * Copyright (C) 2015-2018 Apple Inc. All right reserved.
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
#include <wtf/NeverDestroyed.h>

#if ENABLE(MATHML)
#include "MathMLNames.h"
#endif

namespace WebCore {

using namespace SVGNames;

static const HashSet<String, ASCIICaseInsensitiveHash>& supportedSVGFeatures()
{
    static NeverDestroyed<HashSet<String, ASCIICaseInsensitiveHash>> features = [] {
        static const char* const features10[] = {
            "dom",
            "dom.svg",
            "dom.svg.static",
            "svg",
            "svg.static",
        };
        static const char* const features11[] = {
            "animation",
            "basegraphicsattribute",
            "basicclip",
            "basicfilter",
            "basicpaintattribute",
            "basicstructure",
            "basictext",
            "clip",
            "conditionalprocessing",
            "containerattribute",
            "coreattribute",
            "cursor",
            "documenteventsattribute",
            "extensibility",
            "externalresourcesrequired",
            "filter",
            "gradient",
            "graphicaleventsattribute",
            "graphicsattribute",
            "hyperlinking",
            "image",
            "marker",
            "mask",
            "opacityattribute",
            "paintattribute",
            "pattern",
            "script",
            "shape",
            "structure",
            "style",
            "svg-animation",
            "svgdom-animation",
            "text",
            "view",
            "viewportattribute",
            "xlinkattribute",
            "basicfont",
            "font",
            "svg",
            "svg-static",
            "svgdom",
            "svgdom-static",
        };
        HashSet<String, ASCIICaseInsensitiveHash> set;
        for (auto& feature : features10)
            set.add(makeString("org.w3c.", feature));
        for (auto& feature : features11)
            set.add(makeString("http://www.w3.org/tr/svg11/feature#", feature));
        return set;
    }();
    return features;
}

SVGTests::SVGTests(SVGElement* contextElement)
    : m_contextElement(*contextElement)
    , m_requiredFeatures(SVGStringList::create(contextElement))
    , m_requiredExtensions(SVGStringList::create(contextElement))
    , m_systemLanguage(SVGStringList::create(contextElement))
{
    static std::once_flag onceFlag;
    std::call_once(onceFlag, [] {
        PropertyRegistry::registerProperty<SVGNames::requiredFeaturesAttr, &SVGTests::m_requiredFeatures>();
        PropertyRegistry::registerProperty<SVGNames::requiredExtensionsAttr, &SVGTests::m_requiredExtensions>();
        PropertyRegistry::registerProperty<SVGNames::systemLanguageAttr, &SVGTests::m_systemLanguage>();
    });
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
    for (auto& feature : m_requiredFeatures->items()) {
        if (feature.isEmpty() || !supportedSVGFeatures().contains(feature))
            return false;
    }
    for (auto& language : m_systemLanguage->items()) {
        if (language != defaultLanguage().substring(0, 2))
            return false;
    }
    for (auto& extension : m_requiredExtensions->items()) {
        if (!hasExtension(extension))
            return false;
    }
    return true;
}

void SVGTests::parseAttribute(const QualifiedName& attributeName, const AtomString& value)
{
    if (attributeName == requiredFeaturesAttr)
        m_requiredFeatures->reset(value);
    if (attributeName == requiredExtensionsAttr)
        m_requiredExtensions->reset(value);
    if (attributeName == systemLanguageAttr)
        m_systemLanguage->reset(value);
}

void SVGTests::svgAttributeChanged(const QualifiedName& attrName)
{
    if (!PropertyRegistry::isKnownAttribute(attrName))
        return;

    if (!m_contextElement.isConnected())
        return;
    m_contextElement.invalidateStyleAndRenderersForSubtree();
}

void SVGTests::addSupportedAttributes(HashSet<QualifiedName>& supportedAttributes)
{
    supportedAttributes.add(requiredFeaturesAttr);
    supportedAttributes.add(requiredExtensionsAttr);
    supportedAttributes.add(systemLanguageAttr);
}

bool SVGTests::hasFeatureForLegacyBindings(const String& feature, const String& version)
{
    // FIXME: This function is here only to be exposed in the Objective-C and GObject bindings for both Node and DOMImplementation.
    // It's likely that we can just remove this and instead have the bindings return true unconditionally.
    // This is what the DOMImplementation function now does in JavaScript as is now suggested in the DOM specification.
    // The behavior implemented below is quirky, but preserves what WebKit has done for at least the last few years.

    bool hasSVG10FeaturePrefix = startsWithLettersIgnoringASCIICase(feature, "org.w3c.dom.svg") || startsWithLettersIgnoringASCIICase(feature, "org.w3c.svg");
    bool hasSVG11FeaturePrefix = startsWithLettersIgnoringASCIICase(feature, "http://www.w3.org/tr/svg");

    // We don't even try to handle feature names that don't look like the SVG ones, so just return true for all of those.
    if (!(hasSVG10FeaturePrefix || hasSVG11FeaturePrefix))
        return true;

    // If the version number matches the style of the feature name, then use the set to see if the feature is supported.
    if (version.isEmpty() || (hasSVG10FeaturePrefix && version == "1.0") || (hasSVG11FeaturePrefix && version == "1.1"))
        return supportedSVGFeatures().contains(feature);

    return false;
}

}
