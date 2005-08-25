/*
    Copyright (C) 2004-2005 Nikolas Zimmermann <wildfox@kde.org>
                  2004-2005 Rob Buis <buis@kde.org>

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

#include <ksvg2/ecma/GlobalObject.h>
#include "CDFInterface.h"
#include <ksvg2/ecma/EcmaInterface.h>
#include "SVGRenderStyle.h"

#include <kdom/impl/domattrs.h>
#include <kdom/css/impl/cssvalues.h>
#include <kdom/css/impl/cssproperties.h>

// The auto-generated parts
#include "svgtags.h"
#include "svgattrs.h"
#include <ksvg2/css/impl/cssvalues.h>
#include <ksvg2/css/impl/cssproperties.h>

using namespace KSVG;

CDFInterface::CDFInterface()
{
}

CDFInterface::~CDFInterface()
{
}

const char *CDFInterface::getValueName(KDOM::NodeImpl::Id id) const
{
	if(id > CSS_VAL_MAX && id <= SVGCSS_VAL_MAX)
		return KSVG::getValueName(id - CSS_VAL_MAX);
	
	return KDOM::CDFInterface::getValueName(id);
}

const char *CDFInterface::getPropertyName(KDOM::NodeImpl::Id id) const
{
	if(id > CSS_PROP_MAX && id <= SVGCSS_PROP_MAX)
		return KSVG::getPropertyName(id - CSS_PROP_MAX);

	return KDOM::CDFInterface::getPropertyName(id);
}
		
int CDFInterface::getValueID(const char *valStr, int len) const
{
	int ret = KSVG::getValueID(valStr, len);
	if(ret)
		return ret;

	return KDOM::CDFInterface::getValueID(valStr, len);
}

int CDFInterface::getPropertyID(const char *propStr, int len) const
{
	int ret = KSVG::getPropertyID(propStr, len);
	if(ret)
		return ret;

	return KDOM::CDFInterface::getPropertyID(propStr, len);
}

KDOM::RenderStyle *CDFInterface::renderStyle() const
{
	return new KSVG::SVGRenderStyle();
}

const char *CDFInterface::getTagName(KDOM::NodeImpl::Id id) const
{
	return KSVG::getTagName(id);
}

const char *CDFInterface::getAttrName(KDOM::NodeImpl::Id id) const
{
	if(id > ATTR_LAST_DOMATTR && id <= ATTR_LAST_SVGATTR)
		return KSVG::getAttrName(id - ATTR_LAST_DOMATTR);

	return KDOM::CDFInterface::getAttrName(id);
}
		
int CDFInterface::getTagID(const char *tagStr, int len) const
{
	return KSVG::getTagID(tagStr, len);
}

int CDFInterface::getAttrID(const char *attrStr, int len) const
{
	int ret = KSVG::getAttrID(attrStr, len);
	if(ret > ATTR_LAST_DOMATTR)
		return ret;

	return KDOM::CDFInterface::getAttrID(attrStr, len);
}
#if 0
KDOM::GlobalObject *CDFInterface::globalObject(KDOM::DocumentImpl *doc) const
{
	return new KSVG::GlobalObject(doc);
}
#endif
// vim:ts=4:noet
