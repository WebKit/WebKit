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

#include "kdom.h"
#include "Ecma.h"
#include "NodeImpl.h"
#include "Document.h"
#include "LSOutput.h"
#include "DOMString.h"
#include "KDOMParser.h"
#include "LSException.h"
#include "LSSerializer.h"
#include "LSExceptionImpl.h"
#include "DOMConfiguration.h"
#include "LSSerializerImpl.h"

#include "LSConstants.h"
#include "LSSerializer.lut.h"
using namespace KDOM;
using namespace KJS;

/*
@begin LSSerializer::s_hashTable 5
 domConfig		LSSerializerConstants::DomConfig			DontDelete|ReadOnly
 newLine		LSSerializerConstants::NewLine				DontDelete
 filter			LSSerializerConstants::Filter				DontDelete
@end
@begin LSSerializerProto::s_hashTable 5
 write				LSSerializerConstants::Write			DontDelete|Function 2
 writeToURI			LSSerializerConstants::WriteToURI		DontDelete|Function 2
 writeToString		LSSerializerConstants::WriteToString	DontDelete|Function 1
@end
*/

KDOM_IMPLEMENT_PROTOTYPE("LSSerializer", LSSerializerProto, LSSerializerProtoFunc)

ValueImp *LSSerializer::getValueProperty(ExecState *exec, int token) const
{
	KDOM_ENTER_SAFE

	switch(token)
	{
		case LSSerializerConstants::DomConfig:
			return safe_cache<DOMConfiguration>(exec, domConfig());
		case LSSerializerConstants::NewLine:
			return getDOMString(newLine());
		/*case LSSerializerConstants::Filter:
			return getDOMString(nodeValue());*/
		default:
			kdWarning() << "Unhandled token in " << k_funcinfo << " : " << token << endl;
	}

	KDOM_LEAVE_SAFE(LSException)
	return Undefined();
}

void LSSerializer::putValueProperty(ExecState *exec, int token, ValueImp *value, int)
{
	KDOM_ENTER_SAFE

	switch(token)
	{
		case LSSerializerConstants::Filter:
			break;
		case LSSerializerConstants::NewLine:
			setNewLine(toDOMString(exec, value));
			break;
		default:
			kdWarning() << "Unhandled token in " << k_funcinfo << " : " << token << endl;
	}

	KDOM_LEAVE_SAFE(LSException)
}

ValueImp *LSSerializerProtoFunc::callAsFunction(ExecState *exec, ObjectImp *thisObj, const List &args)
{
	KDOM_CHECK_THIS(LSSerializer)
	KDOM_ENTER_SAFE

	switch(id)
	{
		case LSSerializerConstants::Write:
		{
			Node nodeArg = ecma_cast<Node>(exec, args[0], &toNode);
			LSOutput output = ecma_cast<LSOutput>(exec, args[1], &toLSOutput);
			return KJS::Boolean(obj.write(nodeArg, output));
		}
		case LSSerializerConstants::WriteToURI:
		{
			Node nodeArg = ecma_cast<Node>(exec, args[0], &toNode);
			DOMString uri = toDOMString(exec, args[1]);
			return KJS::Boolean(obj.writeToURI(nodeArg, uri));
		}
		case LSSerializerConstants::WriteToString:
		{
			Node nodeArg = ecma_cast<Node>(exec, args[0], &toNode);
			return getDOMString(obj.writeToString(nodeArg));
		}
		default:
			kdWarning() << "Unhandled function id in " << k_funcinfo << " : " << id << endl;
	}

	KDOM_LEAVE_CALL_SAFE(LSException)
	return Undefined();
}

LSSerializer LSSerializer::null;

LSSerializer::LSSerializer() : d(0)
{
}

LSSerializer::LSSerializer(LSSerializerImpl *i) : d(i)
{
	if(d)
		d->ref();
}

LSSerializer::LSSerializer(const LSSerializer &other) : d(0)
{
	(*this) = other;
}

KDOM_IMPL_DTOR_ASSIGN_OP(LSSerializer)

DOMConfiguration LSSerializer::domConfig() const
{
	if(!d)
		return DOMConfiguration::null;

	return DOMConfiguration(d->domConfig());
}

DOMString LSSerializer::newLine() const
{
	if(!d)
		return DOMString();

	return d->newLine();
}

void LSSerializer::setNewLine(const DOMString &newLine)
{
	if(d)
		d->setNewLine(newLine);
}

bool LSSerializer::write(const Node &nodeArg, const LSOutput &output)
{
	if(!d)
		return false;

	return d->write(static_cast<NodeImpl *>(nodeArg.handle()), output.handle());
}

bool LSSerializer::writeToURI(const Node &nodeArg, const DOMString &uri)
{
	if(!d)
		return false;

	return d->writeToURI(static_cast<NodeImpl *>(nodeArg.handle()), uri);
}

DOMString LSSerializer::writeToString(const Node &nodeArg)
{
	if(!d)
		return DOMString();

	return d->writeToString(static_cast<NodeImpl *>(nodeArg.handle()));
}

// vim:ts=4:noet
