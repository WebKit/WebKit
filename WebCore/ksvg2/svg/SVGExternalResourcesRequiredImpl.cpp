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
#include <kdom/core/AttrImpl.h>

#include "SVGNames.h"
#include "SVGHelper.h"
#include "SVGElementImpl.h"
#include "SVGAnimatedBooleanImpl.h"
#include "SVGExternalResourcesRequiredImpl.h"

using namespace KSVG;

SVGExternalResourcesRequiredImpl::SVGExternalResourcesRequiredImpl()
{
    m_external = 0;
}

SVGExternalResourcesRequiredImpl::~SVGExternalResourcesRequiredImpl()
{
    if(m_external)
        m_external->deref();
}

SVGAnimatedBooleanImpl *SVGExternalResourcesRequiredImpl::externalResourcesRequired() const
{
    return lazy_create<SVGAnimatedBooleanImpl>(m_external, static_cast<const SVGStyledElementImpl *>(0));
}

bool SVGExternalResourcesRequiredImpl::parseMappedAttribute(KDOM::MappedAttributeImpl *attr)
{
    KDOM::DOMString value(attr->value());
    if (attr->name() == SVGNames::externalResourcesRequiredAttr) {
        externalResourcesRequired()->setBaseVal(value == "true");
        return true;
    }

    return false;
}

// vim:ts=4:noet
