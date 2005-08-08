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

#include <kdom/ecma/Ecma.h>
#include <kdom/DOMException.h>
#include <kdom/impl/DocumentImpl.h>

#include "SVGElement.h"
#include "SVGUseElement.h"
#include "SVGElementInstance.h"
#include "SVGElementInstanceList.h"

#include "SVGConstants.h"
#include "SVGElementInstanceImpl.h"
#include "SVGElementInstanceListImpl.h"
#include "SVGElementInstance.lut.h"
using namespace KSVG;
using namespace KDOM;

/*
@begin SVGElementInstance::s_hashTable 9
 correspondingElement		SVGElementInstanceConstants::CorrespondingElement	DontDelete|ReadOnly
 correspondingUseElement	SVGElementInstanceConstants::CorrespondingUseElement	DontDelete|ReadOnly
 parentNode		SVGElementInstanceConstants::ParentNode			DontDelete|ReadOnly
 childNodes		SVGElementInstanceConstants::ChildNodes			DontDelete|ReadOnly
 firstChild		SVGElementInstanceConstants::FirstChild			DontDelete|ReadOnly
 lastChild		SVGElementInstanceConstants::LastChild			DontDelete|ReadOnly
 previousSibling	SVGElementInstanceConstants::PreviousSibling	DontDelete|ReadOnly
 nextSibling	SVGElementInstanceConstants::NextSibling		DontDelete|ReadOnly
@end
*/

ValueImp *SVGElementInstance::getValueProperty(ExecState *exec, int token) const
{
	KDOM_ENTER_SAFE

	switch(token)
	{
		case SVGElementInstanceConstants::CorrespondingElement:
			return getDOMNode(exec, correspondingElement());
		case SVGElementInstanceConstants::CorrespondingUseElement:
			return safe_cache<SVGUseElement>(exec, correspondingUseElement());
		case SVGElementInstanceConstants::ParentNode:
			return safe_cache<SVGElementInstance>(exec, parentNode());
		case SVGElementInstanceConstants::ChildNodes:
			return safe_cache<SVGElementInstanceList>(exec, childNodes());
		case SVGElementInstanceConstants::FirstChild:
			return safe_cache<SVGElementInstance>(exec, firstChild());
		case SVGElementInstanceConstants::LastChild:
			return safe_cache<SVGElementInstance>(exec, lastChild());
		case SVGElementInstanceConstants::PreviousSibling:
			return safe_cache<SVGElementInstance>(exec, previousSibling());
		case SVGElementInstanceConstants::NextSibling:
			return safe_cache<SVGElementInstance>(exec, nextSibling());
		default:
			kdWarning() << "Unhandled token in " << k_funcinfo << " : " << token << endl;
	}

	KDOM_LEAVE_SAFE(KDOM::DOMException)
	return Undefined();
};

// The qdom way...
#define impl (static_cast<SVGElementInstanceImpl *>(d))

SVGElementInstance SVGElementInstance::null;

SVGElementInstance::SVGElementInstance() : KDOM::EventTarget()
{
}

SVGElementInstance::SVGElementInstance(SVGElementInstanceImpl *i)
: KDOM::EventTarget(i)
{
}

SVGElementInstance::SVGElementInstance(const SVGElementInstance &other)
: KDOM::EventTarget()
{
	(*this) = other;
}

SVGElementInstance::~SVGElementInstance()
{
}

SVGElementInstance &SVGElementInstance::operator=(const SVGElementInstance &other)
{
	KDOM::EventTarget::operator=(other);
	return *this;
}

SVGElement SVGElementInstance::correspondingElement() const
{
	if(!d)
		return SVGElement::null;

	return SVGElement(impl->correspondingElement());
}

SVGUseElement SVGElementInstance::correspondingUseElement() const
{
	if(!d)
		return SVGUseElement::null;

	return SVGUseElement(impl->correspondingUseElement());
	
}

SVGElementInstance SVGElementInstance::parentNode() const
{
	if(!d)
		return SVGElementInstance::null;

	return SVGElementInstance(impl->parentNode());
}

SVGElementInstanceList SVGElementInstance::childNodes() const
{
	if(!d)
		return SVGElementInstanceList::null;

	return SVGElementInstanceList(impl->childNodes());
}

SVGElementInstance SVGElementInstance::firstChild() const
{
	if(!d)
		return SVGElementInstance::null;

	return SVGElementInstance(impl->firstChild());
}

SVGElementInstance SVGElementInstance::lastChild() const
{
	if(!d)
		return SVGElementInstance::null;

	return SVGElementInstance(impl->lastChild());
}

SVGElementInstance SVGElementInstance::previousSibling() const
{
	if(!d)
		return SVGElementInstance::null;

	return SVGElementInstance(impl->previousSibling());
}

SVGElementInstance SVGElementInstance::nextSibling() const
{
	if(!d)
		return SVGElementInstance::null;

	return SVGElementInstance(impl->nextSibling());
}

// vim:ts=4:noet
