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

#if ENABLE(SVG)
#include "SVGTests.h"

#include "Attribute.h"
#include "DOMImplementation.h"
#include "Language.h"
#include "SVGNames.h"
#include "SVGStringList.h"

namespace WebCore {

SVGTests::SVGTests()
    : m_features(SVGNames::requiredFeaturesAttr)
    , m_extensions(SVGNames::requiredExtensionsAttr)
    , m_systemLanguage(SVGNames::systemLanguageAttr)
{
}

bool SVGTests::hasExtension(const String&) const
{
    // FIXME: Implement me!
    return false;
}

bool SVGTests::isValid() const
{
    unsigned featuresSize = m_features.size();
    for (unsigned i = 0; i < featuresSize; ++i) {
        String value = m_features.at(i);
        if (value.isEmpty() || !DOMImplementation::hasFeature(value, String()))
            return false;
    }

    unsigned systemLanguageSize = m_systemLanguage.size();
    for (unsigned i = 0; i < systemLanguageSize; ++i) {
        String value = m_systemLanguage.at(i);
        if (value != defaultLanguage().substring(0, 2))
            return false;
    }

    if (!m_extensions.isEmpty())
        return false;

    return true;
}

bool SVGTests::parseMappedAttribute(Attribute* attr)
{
    if (attr->name() == SVGNames::requiredFeaturesAttr) {
        m_features.reset(attr->value());
        return true;
    } else if (attr->name() == SVGNames::requiredExtensionsAttr) {
        m_extensions.reset(attr->value());
        return true;
    } else if (attr->name() == SVGNames::systemLanguageAttr) {
        m_systemLanguage.reset(attr->value());
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

}

#endif // ENABLE(SVG)
