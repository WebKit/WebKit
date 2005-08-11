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

#include "Ecma.h"
#include "DOMException.h"
#include "CharacterData.h"

#include "DOMConstants.h"
#include "CharacterData.lut.h"
#include "CharacterDataImpl.h"
using namespace KDOM;
using namespace KJS;

/*
@begin CharacterData::s_hashTable 3
 data			CharacterDataConstants::Data			DontDelete
 length			CharacterDataConstants::Length			DontDelete|ReadOnly
@end
@begin CharacterDataProto::s_hashTable 7
 setData		CharacterDataConstants::SetData			DontDelete|Function 1
 substringData	CharacterDataConstants::SubstringData	DontDelete|Function 2
 appendData		CharacterDataConstants::AppendData		DontDelete|Function 1
 insertData		CharacterDataConstants::InsertData		DontDelete|Function 2
 deleteData		CharacterDataConstants::DeleteData		DontDelete|Function 2
 replaceData	CharacterDataConstants::ReplaceData		DontDelete|Function 3
@end
*/

KDOM_IMPLEMENT_PROTOTYPE("CharacterData", CharacterDataProto, CharacterDataProtoFunc)

ValueImp *CharacterData::getValueProperty(ExecState *exec, int token) const
{
	KDOM_ENTER_SAFE
	
	switch(token)
	{
		case CharacterDataConstants::Data:
			return getDOMString(data());
		case CharacterDataConstants::Length:
			return Number(length());
		default:
			kdWarning() << "Unhandled token in " << k_funcinfo << " : " << token << endl;
	}

	KDOM_LEAVE_SAFE(DOMException)
	return Undefined();
}

void CharacterData::putValueProperty(ExecState *exec, int token, ValueImp *value, int)
{
	KDOM_ENTER_SAFE

	switch(token)
	{
		case CharacterDataConstants::Data:
		{
			DOMString data = toDOMString(exec, value);
			setData(data);
			break;
		}
		default:
			kdWarning() << "Unhandled token in " << k_funcinfo << " : " << token << endl;
	}

	KDOM_LEAVE_SAFE(DOMException)
}

ValueImp *CharacterDataProtoFunc::callAsFunction(ExecState *exec, ObjectImp *thisObj, const List &args)
{
	KDOM_CHECK_THIS(CharacterData)
	KDOM_ENTER_SAFE

	switch(id)
	{
		case CharacterDataConstants::SetData:
		{
			DOMString data = toDOMString(exec, args[0]);
			obj.setData(data);
			return Undefined();
		}
		case CharacterDataConstants::SubstringData:
		{
			unsigned long offset = args[0]->toUInt32(exec);
			unsigned long count = args[1]->toUInt32(exec);
			return getDOMString(obj.substringData(offset, count));
		}
		case CharacterDataConstants::AppendData:
		{
			DOMString arg = toDOMString(exec, args[0]);
			obj.appendData(arg);
			return Undefined();
		}
		case CharacterDataConstants::InsertData:
		{
			unsigned long offset = args[0]->toUInt32(exec);
			DOMString arg = toDOMString(exec, args[1]);
			obj.insertData(offset, arg);
			return Undefined();
		}
		case CharacterDataConstants::DeleteData:
		{
			unsigned long offset = args[0]->toUInt32(exec);
			unsigned long count = args[1]->toUInt32(exec);
			obj.deleteData(offset, count);
			return Undefined();
		}
		case CharacterDataConstants::ReplaceData:
		{
			unsigned long offset = args[0]->toUInt32(exec);
			unsigned long count = args[1]->toUInt32(exec);
			DOMString arg = toDOMString(exec, args[2]);
			obj.replaceData(offset, count, arg);
			return Undefined();
		}
		default:
			kdWarning() << "Unhandled function id in " << k_funcinfo << " : " << id << endl;
	}

	KDOM_LEAVE_CALL_SAFE(DOMException)
	return Undefined();
}

// The qdom way...
#define impl (static_cast<CharacterDataImpl *>(d))

CharacterData CharacterData::null;

CharacterData::CharacterData() : Node()
{
}

CharacterData::CharacterData(CharacterDataImpl *i) : Node(i)
{
}

CharacterData::CharacterData(const CharacterData &other) : Node()
{
	(*this) = other;
}

CharacterData::CharacterData(const Node &other) : Node()
{
	(*this) = other;
}

CharacterData::~CharacterData()
{
}

CharacterData &CharacterData::operator=(const CharacterData &other)
{
	Node::operator=(other);
	return *this;
}

CharacterData &CharacterData::operator=(const Node &other)
{
	NodeImpl *ohandle = static_cast<NodeImpl *>(other.handle());
	if(d != ohandle)
	{
		if(!ohandle || (ohandle->nodeType() != CDATA_SECTION_NODE &&
						ohandle->nodeType() != TEXT_NODE &&
						ohandle->nodeType() != COMMENT_NODE))
		{
			if(d)
				d->deref();
				
			d = 0;
		}
		else
			Node::operator=(other);
	}

	return *this;
}

DOMString CharacterData::data() const
{
	if(!d)
		return DOMString();

	return DOMString(impl->data());
}

void CharacterData::setData(const DOMString &data)
{
	if(!d)
		return;

	return impl->setData(data.implementation());
}

unsigned long CharacterData::length() const
{
	if(!d)
		return 0;

	return impl->length();
}

DOMString CharacterData::substringData(unsigned long offset, unsigned long count)
{
	if(!d)
		return DOMString();

	return DOMString(impl->substringData(offset, count));
}

void CharacterData::appendData(const DOMString &arg)
{
	if(!d)
		return;

	impl->appendData(arg.implementation());
}

void CharacterData::insertData(unsigned long offset, const DOMString &arg)
{
	if(!d)
		return;

	impl->insertData(offset, arg.implementation());
}

void CharacterData::deleteData(unsigned long offset, unsigned long count)
{
	if(!d)
		return;

	return impl->deleteData(offset, count);
}

void CharacterData::replaceData(unsigned long offset, unsigned long count, const DOMString &arg)
{
	if(!d)
		return;

	impl->replaceData(offset, count, arg.implementation());
}

// vim:ts=4:noet
