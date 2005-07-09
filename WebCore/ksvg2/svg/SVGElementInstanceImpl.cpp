/*
    Copyright (C) 2004 Nikolas Zimmermann <wildfox@kde.org>
				  2004 Rob Buis <buis@kde.org>

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

#include "SVGElementImpl.h"
#include "SVGUseElementImpl.h"
#include "SVGElementInstanceImpl.h"
#include "SVGElementInstanceListImpl.h"

using namespace KSVG;

SVGElementInstanceImpl::SVGElementInstanceImpl() : KDOM::EventTargetImpl()
{
}

SVGElementInstanceImpl::~SVGElementInstanceImpl()
{
}

SVGElementImpl *SVGElementInstanceImpl::correspondingElement() const
{
	return 0;
}

SVGUseElementImpl *SVGElementInstanceImpl::correspondingUseElement() const
{
	return 0;
}

SVGElementInstanceImpl *SVGElementInstanceImpl::parentNode() const
{
	return 0;
}

SVGElementInstanceListImpl *SVGElementInstanceImpl::childNodes() const
{
	return 0;
}

SVGElementInstanceImpl *SVGElementInstanceImpl::previousSibling() const
{
	return 0;
}

SVGElementInstanceImpl *SVGElementInstanceImpl::nextSibling() const
{
	return 0;
}

SVGElementInstanceImpl *SVGElementInstanceImpl::firstChild() const
{
	return 0;
}

SVGElementInstanceImpl *SVGElementInstanceImpl::lastChild() const
{
	return 0;
}

// vim:ts=4:noet
