/*
    Copyright (C) 2004, 2005 Nikolas Zimmermann <wildfox@kde.org>
                  2004, 2005 Rob, 2006 Buis <buis@kde.org>

    This file is part of the KDE project

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Library General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Library General Public License for more details.

    You should have received a copy of the GNU Library General Public License
    along with this library; see the file COPYING.LIB.  If not, write to
    the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
    Boston, MA 02111-1307, USA.
*/

#include "config.h"
#ifdef SVG_SUPPORT
#include "SVGTests.h"

#include "DOMImplementation.h"
#include "Language.h"
#include "SVGElement.h"
#include "SVGNames.h"
#include "SVGStringList.h"

namespace WebCore {

SVGTests::SVGTests()
{
}

SVGTests::~SVGTests()
{
}

SVGStringList* SVGTests::requiredFeatures() const
{
    if (!m_features)
        m_features = new SVGStringList();

    return m_features.get();
}

SVGStringList* SVGTests::requiredExtensions() const
{
    if (!m_extensions)
        m_extensions = new SVGStringList();

    return m_extensions.get();
}

SVGStringList* SVGTests::systemLanguage() const
{
    if (!m_systemLanguage)
        m_systemLanguage = new SVGStringList();

    return m_systemLanguage.get();
}

bool SVGTests::hasExtension(const String&) const
{
    return false;
}

bool SVGTests::isValid() const
{
    ExceptionCode ec = 0;

    SVGStringList* list = requiredFeatures();
    for (unsigned long i = 0; i < list->numberOfItems(); i++) {
        String value = list->getItem(i, ec);
        if (value.isEmpty() || !DOMImplementation::instance()->hasFeature(value, String()))
            return false;
    }

    list = systemLanguage();
    for (unsigned long i = 0; i < list->numberOfItems(); i++)
        if (list->getItem(i, ec) != defaultLanguage().substring(0, 2))
            return false;

    list = requiredExtensions();
    if (list->numberOfItems() > 0)
        return false;

    return true;
}

bool SVGTests::parseMappedAttribute(MappedAttribute* attr)
{
    const String& value = attr->value();
    if (attr->name() == SVGNames::requiredFeaturesAttr) {
        requiredFeatures()->reset(value);
        return true;
    } else if (attr->name() == SVGNames::requiredExtensionsAttr) {
        requiredExtensions()->reset(value);
        return true;
    } else if (attr->name() == SVGNames::systemLanguageAttr) {
        systemLanguage()->reset(value);
        return true;
    }
    
    return false;
}

}

// vim:ts=4:noet
#endif // SVG_SUPPORT
