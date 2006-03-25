/*
    Copyright (C) 2004, 2005 Nikolas Zimmermann <wildfox@kde.org>
                  2004, 2005 Rob Buis <buis@kde.org>

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
#if SVG_SUPPORT
#include "SVGTests.h"

#include "Language.h"
#include "SVGDOMImplementation.h"
#include "SVGElement.h"
#include "SVGHelper.h"
#include "SVGNames.h"
#include "SVGStringList.h"
#include "Attr.h"

namespace WebCore {

SVGTests::SVGTests()
{
}

SVGTests::~SVGTests()
{
}

SVGStringList *SVGTests::requiredFeatures() const
{
    return lazy_create<SVGStringList>(m_features);
}

SVGStringList *SVGTests::requiredExtensions() const
{
    return lazy_create<SVGStringList>(m_extensions);
}

SVGStringList *SVGTests::systemLanguage() const
{
    return lazy_create<SVGStringList>(m_systemLanguage);
}

bool SVGTests::hasExtension(StringImpl *) const
{
    return false;
}

bool SVGTests::isValid() const
{
    SVGStringList *list = requiredFeatures();
    for(unsigned long i = 0;i < list->numberOfItems();i++)
    {
        String value = String(list->getItem(i));
        if(value.isEmpty() || !SVGDOMImplementation::self()->hasFeature(value.impl(), 0))
            return false;
    }

    list = systemLanguage();
    for (unsigned long i = 0; i < list->numberOfItems(); i++)
        if (!equal(list->getItem(i), defaultLanguage().substring(0, 2).impl()))
            return false;

    list = requiredExtensions();
    if(list->numberOfItems() > 0)
        return false;

    return true;
}

bool SVGTests::parseMappedAttribute(MappedAttribute *attr)
{
    const String& value = attr->value();
    if (attr->name() == SVGNames::requiredFeaturesAttr) {
        requiredFeatures()->reset(value.deprecatedString());
        return true;
    } else if (attr->name() == SVGNames::requiredExtensionsAttr) {
        requiredExtensions()->reset(value.deprecatedString());
        return true;
    } else if (attr->name() == SVGNames::systemLanguageAttr) {
        systemLanguage()->reset(value.deprecatedString());
        return true;
    }
    
    return false;
}

}

// vim:ts=4:noet
#endif // SVG_SUPPORT
