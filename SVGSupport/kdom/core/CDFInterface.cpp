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

#include "RenderStyle.h"
#include <kdom/ecma/GlobalObject.h>
#include "CDFInterface.h"

// The auto-generated parts
#include "domattrs.h"
#include <kdom/css/cssvalues.h>
#include <kdom/css/cssproperties.h>

using namespace KDOM;

CDFInterface::CDFInterface()
{
}

CDFInterface::~CDFInterface()
{
}

const char *CDFInterface::getValueName(NodeImpl::Id id) const
{
    return KDOM::getValueName(id);
}

const char *CDFInterface::getPropertyName(NodeImpl::Id id) const
{
    return KDOM::getPropertyName(id);
}
        
int CDFInterface::getValueID(const char *valStr, int len) const
{
    return KDOM::getValueID(valStr, len);
}

int CDFInterface::getPropertyID(const char *propStr, int len) const
{
    return KDOM::getPropertyID(propStr, len);
}

RenderStyle *CDFInterface::renderStyle() const
{
    return new RenderStyle();
}

bool CDFInterface::cssPropertyApplyFirst(int id) const
{
    // give special priority to font-xxx, color properties
    switch(id)
    {
        case CSS_PROP_FONT_STYLE:
        case CSS_PROP_FONT_SIZE:
        case CSS_PROP_FONT_WEIGHT:
        case CSS_PROP_FONT_FAMILY:
        case CSS_PROP_FONT:
        case CSS_PROP_COLOR:
        case CSS_PROP_DIRECTION:
        case CSS_PROP_BACKGROUND_IMAGE:
        case CSS_PROP_DISPLAY:
            // these have to be applied first, because other properties use
            // the computed values of these properties.
            return true;
        default:
            return false;
    }
}

const char *CDFInterface::getTagName(NodeImpl::Id) const
{
    return "";
}

const char *CDFInterface::getAttrName(NodeImpl::Id id) const
{
    return KDOM::getAttrName(id);
}
        
int CDFInterface::getTagID(const char *, int) const
{
    return 0;
}

int CDFInterface::getAttrID(const char *attrStr, int len) const
{
    return KDOM::getAttrID(attrStr, len);
}

GlobalObject *CDFInterface::globalObject(DocumentImpl *doc) const
{
    return new GlobalObject(doc);
}

// vim:ts=4:noet
