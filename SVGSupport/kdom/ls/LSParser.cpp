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

#include "kdom.h"
#include "Ecma.h"
#include "LSInput.h"
#include "LSParser.h"
#include "NodeImpl.h"
#include "Document.h"
#include "DOMString.h"
#include "KDOMParser.h"
#include "LSException.h"
#include "DOMException.h"
#include "LSParserImpl.h"
#include "LSParserFilter.h"
#include "LSExceptionImpl.h"
#include "DOMConfiguration.h"
#include "LSParserFilterImpl.h"

#include "LSConstants.h"
#include "LSParser.lut.h"
using namespace KDOM;
using namespace KJS;

/*
@begin LSParser::s_hashTable 5
 domConfig		LSParserConstants::DomConfig			DontDelete|ReadOnly
 filter			LSParserConstants::Filter				DontDelete
 async			LSParserConstants::Async				DontDelete|ReadOnly
 busy			LSParserConstants::Busy					DontDelete|ReadOnly
@end
@begin LSParserProto::s_hashTable 5
 parse				LSParserConstants::Parse			DontDelete|Function 1
 parseURI			LSParserConstants::ParseURI			DontDelete|Function 1
 parseWithContext	LSParserConstants::ParseWithContext	DontDelete|Function 3
 abort				LSParserConstants::Abort			DontDelete|Function 0
@end
*/

KDOM_IMPLEMENT_PROTOTYPE("LSParser", LSParserProto, LSParserProtoFunc)

ValueImp *LSParser::getValueProperty(ExecState *exec, int token) const
{
	KDOM_ENTER_SAFE

	switch(token)
	{
		case LSParserConstants::DomConfig:
			return safe_cache<DOMConfiguration>(exec, domConfig());
		case LSParserConstants::Filter:
			return safe_cache<LSParserFilter>(exec, filter());
		case LSParserConstants::Async:
			return KJS::Boolean(async());
		case LSParserConstants::Busy:
			return KJS::Boolean(busy());
		default:
			kdWarning() << "Unhandled token in " << k_funcinfo << " : " << token << endl;
	}

	KDOM_LEAVE_SAFE(DOMException)
	return Undefined();
}

void LSParser::putValueProperty(ExecState *exec, int token, ValueImp * /* value */, int)
{
	KDOM_ENTER_SAFE

	switch(token)
	{
		case LSParserConstants::Filter:
			break;
		default:
			kdWarning() << "Unhandled token in " << k_funcinfo << " : " << token << endl;
	}

	KDOM_LEAVE_SAFE(DOMException)
}

ValueImp *LSParserProtoFunc::callAsFunction(ExecState *exec, ObjectImp *thisObj, const List &args)
{
	KDOM_CHECK_THIS(LSParser)
	KDOM_ENTER_SAFE
	KDOM_ENTER_SAFE

	switch(id)
	{
		case LSParserConstants::Parse:
		{
			LSInput input = ecma_cast<LSInput>(exec, args[0], &toLSInput);
			return getDOMNode(exec, obj.parse(input));
		}
		case LSParserConstants::ParseURI:
		{
			DOMString uri = toDOMString(exec, args[0]);
			return getDOMNode(exec, obj.parseURI(uri));
		}
		case LSParserConstants::ParseWithContext:
		{
			LSInput input = ecma_cast<LSInput>(exec, args[0], &toLSInput);
			Node contextArg = ecma_cast<Node>(exec, args[1], &toNode);
			unsigned short action = args[2]->toUInt32(exec);
			return getDOMNode(exec, obj.parseWithContext(input, contextArg, action));
		}
		case LSParserConstants::Abort:
		{
			obj.abort();
			return Undefined();
		}
		default:
			kdWarning() << "Unhandled function id in " << k_funcinfo << " : " << id << endl;
	}

	KDOM_LEAVE_CALL_SAFE(DOMException)
	KDOM_LEAVE_CALL_SAFE(LSException)
	return Undefined();
}

// The qdom way...
#define impl (static_cast<LSParserImpl *>(d))

LSParser LSParser::null;

LSParser::LSParser() : EventTarget()
{
}

LSParser::~LSParser()
{
}

LSParser::LSParser(LSParserImpl *i) : EventTarget(i)
{
}

LSParser::LSParser(const LSParser &other) : EventTarget()
{
	(*this) = other;
}

LSParser &LSParser::operator=(const LSParser &other)
{
	EventTarget::operator=(other);
	return *this;
}

DOMConfiguration LSParser::domConfig() const
{
	if(!d)
		return DOMConfiguration::null;

	return DOMConfiguration(impl->domConfig());
}

LSParserFilter LSParser::filter() const
{
	if(!d)
		return LSParserFilter::null;

	return LSParserFilter(impl->filter());
}

void LSParser::setFilter(const LSParserFilter &filter)
{
	if(d)
		impl->setFilter(filter.handle());
}

bool LSParser::busy() const
{
	if(!d)
		return false;

	return impl->busy();
}

bool LSParser::async() const
{
	if(!d)
		return false;

	return impl->async();
}

Document LSParser::parse(const LSInput &input)
{
	if(!d)
		return Document::null;

	return Document(impl->parse(input.handle()));
}

Document LSParser::parseURI(const DOMString &uri)
{
	if(!d)
		return Document::null;

	return Document(impl->parseURI(uri));
}

Node LSParser::parseWithContext(const LSInput &input, const Node &contextArg,
								unsigned short action)
{
	if(!d)
		return Document::null;

	return Node(impl->parseWithContext(input.handle(), static_cast<NodeImpl *>(contextArg.handle()), action));
}

void LSParser::abort() const
{
	if(d)
		impl->abort();
}

// vim:ts=4:noet
