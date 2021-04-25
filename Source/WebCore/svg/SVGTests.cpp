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
#define PREFIX_W3C "org.w3c."
#define PREFIX_SVG11 "http://www.w3.org/tr/svg11/feature#"
    static NeverDestroyed<HashSet<String, ASCIICaseInsensitiveHash>> features = [] {
        static const ASCIILiteral features10[] = {
            PREFIX_W3C "dom"_s,
            PREFIX_W3C "dom.svg"_s,
            PREFIX_W3C "dom.svg.static"_s,
            PREFIX_W3C "svg"_s,
            PREFIX_W3C "svg.static"_s,
        };
        static const ASCIILiteral features11[] = {
            PREFIX_SVG11 "animation"_s,
            PREFIX_SVG11 "basegraphicsattribute"_s,
            PREFIX_SVG11 "basicclip"_s,
            PREFIX_SVG11 "basicfilter"_s,
            PREFIX_SVG11 "basicpaintattribute"_s,
            PREFIX_SVG11 "basicstructure"_s,
            PREFIX_SVG11 "basictext"_s,
            PREFIX_SVG11 "clip"_s,
            PREFIX_SVG11 "conditionalprocessing"_s,
            PREFIX_SVG11 "containerattribute"_s,
            PREFIX_SVG11 "coreattribute"_s,
            PREFIX_SVG11 "cursor"_s,
            PREFIX_SVG11 "documenteventsattribute"_s,
            PREFIX_SVG11 "extensibility"_s,
            PREFIX_SVG11 "externalresourcesrequired"_s,
            PREFIX_SVG11 "filter"_s,
            PREFIX_SVG11 "gradient"_s,
            PREFIX_SVG11 "graphicaleventsattribute"_s,
            PREFIX_SVG11 "graphicsattribute"_s,
            PREFIX_SVG11 "hyperlinking"_s,
            PREFIX_SVG11 "image"_s,
            PREFIX_SVG11 "marker"_s,
            PREFIX_SVG11 "mask"_s,
            PREFIX_SVG11 "opacityattribute"_s,
            PREFIX_SVG11 "paintattribute"_s,
            PREFIX_SVG11 "pattern"_s,
            PREFIX_SVG11 "script"_s,
            PREFIX_SVG11 "shape"_s,
            PREFIX_SVG11 "structure"_s,
            PREFIX_SVG11 "style"_s,
            PREFIX_SVG11 "svg-animation"_s,
            PREFIX_SVG11 "svgdom-animation"_s,
            PREFIX_SVG11 "text"_s,
            PREFIX_SVG11 "view"_s,
            PREFIX_SVG11 "viewportattribute"_s,
            PREFIX_SVG11 "xlinkattribute"_s,
            PREFIX_SVG11 "basicfont"_s,
            PREFIX_SVG11 "font"_s,
            PREFIX_SVG11 "svg"_s,
            PREFIX_SVG11 "svg-static"_s,
            PREFIX_SVG11 "svgdom"_s,
            PREFIX_SVG11 "svgdom-static"_s,
        };
        HashSet<String, ASCIICaseInsensitiveHash> set;
        for (auto& feature : features10)
            set.add(feature);
        for (auto& feature : features11)
            set.add(feature);
        return set;
    }();
#undef PREFIX_W3C
#undef PREFIX_SVG11
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

void SVGTests::addSupportedAttributes(MemoryCompactLookupOnlyRobinHoodHashSet<QualifiedName>& supportedAttributes)
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
