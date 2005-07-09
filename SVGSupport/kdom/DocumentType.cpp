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

// Special case: This class can have a parent, but no children!

#include "Ecma.h"
#include <kdom/Helper.h>
#include "DOMException.h"
#include "NamedNodeMap.h"
#include "DocumentType.h"
#include "LSSerializerImpl.h"

#include <qtextstream.h>

#include "DOMConstants.h"
#include "DocumentType.lut.h"
#include "DocumentTypeImpl.h"
using namespace KDOM;
using namespace KJS;

/*
@begin DocumentType::s_hashTable 7
 name			DocumentTypeConstants::Name				DontDelete|ReadOnly
 entities		DocumentTypeConstants::Entities			DontDelete|ReadOnly
 notations		DocumentTypeConstants::Notations		DontDelete|ReadOnly
 publicId		DocumentTypeConstants::PublicId			DontDelete|ReadOnly
 systemId		DocumentTypeConstants::SystemId			DontDelete|ReadOnly
 internalSubset	DocumentTypeConstants::InternalSubset	DontDelete|ReadOnly
@end
*/

Value DocumentType::getValueProperty(ExecState *exec, int token) const
{
	KDOM_ENTER_SAFE

	switch(token)
	{
		case DocumentTypeConstants::Name:
			return getDOMString(name());
		case DocumentTypeConstants::Entities:
			return safe_cache<NamedNodeMap>(exec, entities());
		case DocumentTypeConstants::Notations:
			return safe_cache<NamedNodeMap>(exec, notations());
		case DocumentTypeConstants::PublicId:
			return getDOMString(publicId());
		case DocumentTypeConstants::SystemId:
			return getDOMString(systemId());
		case DocumentTypeConstants::InternalSubset:
			return getDOMString(internalSubset());
		default:
			kdWarning() << "Unhandled token in " << k_funcinfo << " : " << token << endl;
	}

	KDOM_LEAVE_SAFE(DOMException)
	return Undefined();
}

// The qdom way...
#define impl (static_cast<DocumentTypeImpl *>(d))

DocumentType DocumentType::null;

DocumentType::DocumentType() : Node()
{
}

DocumentType::DocumentType(DocumentTypeImpl *i) : Node(i)
{
}

DocumentType::DocumentType(const DocumentType &other) : Node()
{
	(*this) = other;
}

DocumentType::DocumentType(const Node &other) : Node()
{
	(*this) = other;
}

DocumentType::~DocumentType()
{
}

DocumentType &DocumentType::operator=(const DocumentType &other)
{
	Node::operator=(other);
	return *this;
}

KDOM_NODE_DERIVED_ASSIGN_OP(DocumentType, DOCUMENT_TYPE_NODE)

DOMString DocumentType::name() const
{
	if(!d)
		return DOMString();

	return impl->nodeName();
}

NamedNodeMap DocumentType::entities() const
{
	if(!d)
		return NamedNodeMap::null;

	return NamedNodeMap(impl->entities());
}

NamedNodeMap DocumentType::notations() const
{
	if(!d)
		return NamedNodeMap::null;

	return NamedNodeMap(impl->notations());
}

DOMString DocumentType::publicId() const
{
	if(!d)
		return DOMString();

	return impl->publicId();
}

DOMString DocumentType::systemId() const
{
	if(!d)
		return DOMString();

	return impl->systemId();
}

DOMString DocumentType::internalSubset() const
{
	if(!d)
		return DOMString();

	QString str;
	QTextOStream subset(&str);
	LSSerializerImpl::PrintInternalSubset(subset, static_cast<DocumentTypeImpl *>(handle()));
	return str;
}

// vim:ts=4:noet
