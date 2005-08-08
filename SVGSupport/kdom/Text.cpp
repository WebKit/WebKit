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

#include "Text.h"
#include "Ecma.h"
#include "DOMException.h"

#include "DOMConstants.h"
#include "Text.lut.h"
#include "TextImpl.h"
using namespace KDOM;
using namespace KJS;

/*
@begin Text::s_hashTable 3
 isElementContentWhitespace	TextConstants::IsElementContentWhitespace	DontDelete|ReadOnly
 wholeText					TextConstants::WholeText					DontDelete|ReadOnly
@end
@begin TextProto::s_hashTable 3
 splitText					TextConstants::SplitText					DontDelete|Function 1
 replaceWholeText			TextConstants::ReplaceWholeText				DontDelete|Function 1
@end
*/

KDOM_IMPLEMENT_PROTOTYPE("Text", TextProto, TextProtoFunc)

ValueImp *Text::getValueProperty(ExecState *exec, int token) const
{
	KDOM_ENTER_SAFE

	switch(token)
	{
		case TextConstants::IsElementContentWhitespace:
			return KJS::Boolean(isElementContentWhitespace());
		case TextConstants::WholeText:
			return getDOMString(wholeText());
		default:
			kdWarning() << "Unhandled token in " << k_funcinfo << " : " << token << endl;
	}

	KDOM_LEAVE_SAFE(DOMException)
	return Undefined();
}

ValueImp *TextProtoFunc::callAsFunction(ExecState *exec, ObjectImp *thisObj, const List &args)
{
	KDOM_CHECK_THIS(Text)
	KDOM_ENTER_SAFE

	switch(id)
	{
		case TextConstants::SplitText:
		{
			unsigned long offset = args[0]->toUInt32(exec);
			return getDOMNode(exec, obj.splitText(offset));
		}
		case TextConstants::ReplaceWholeText:
		{
			DOMString content = toDOMString(exec, args[0]);
			return getDOMNode(exec, obj.replaceWholeText(content));
		}
		default:
			kdWarning() << "Unhandled function id in " << k_funcinfo << " : " << id << endl;
	}

	KDOM_LEAVE_CALL_SAFE(DOMException)
	return Undefined();
}

// The qdom way...
#define impl (static_cast<TextImpl *>(d))

Text Text::null;

Text::Text() : CharacterData()
{
}

Text::Text(TextImpl *i) : CharacterData(i)
{
}

Text::Text(const Text &other) : CharacterData()
{
	(*this) = other;
}

Text::Text(const Node &other) : CharacterData()
{
	(*this) = other;
}

Text::~Text()
{
}

Text &Text::operator=(const Text &other)
{
	CharacterData::operator=(other);
	return *this;
}

KDOM_NODE_DERIVED_ASSIGN_OP(Text, TEXT_NODE)

Text Text::splitText(unsigned long offset)
{
	if(!d)
		return Text::null;

	return Text(impl->splitText(offset));
}

bool Text::isElementContentWhitespace() const
{
	if(!d)
		return false;

	return impl->isElementContentWhitespace();
}

DOMString Text::wholeText() const
{
	if(!d)
		return DOMString();

	return impl->wholeText();
}

Text Text::replaceWholeText(const DOMString &content)
{
	if(!d)
		return Text::null;

	return Text(impl->replaceWholeText(content.implementation()));
}

// vim:ts=4:noet
