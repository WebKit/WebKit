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

#include <qtextstream.h>

#include "kdom.h"
#include "Ecma.h"
#include "LSInput.h"
#include "DOMString.h"
#include "LSInputImpl.h"
#include "LSException.h"
#include "LSExceptionImpl.h"

#include "LSConstants.h"
#include "LSInput.lut.h"
using namespace KDOM;
using namespace KJS;

/*
@begin LSInput::s_hashTable 9
 characterStream	LSInputConstants::CharacterStream		DontDelete
 byteStream			LSInputConstants::ByteStream			DontDelete
 stringData			LSInputConstants::StringData			DontDelete
 systemId			LSInputConstants::SystemId				DontDelete
 publicId			LSInputConstants::PublicId				DontDelete
 baseURI			LSInputConstants::BaseURI				DontDelete
 encoding			LSInputConstants::Encoding				DontDelete
 certifiedText		LSInputConstants::CertifiedText			DontDelete
@end
*/

ValueImp *LSInput::getValueProperty(ExecState *exec, int token) const
{
#ifndef APPLE_COMPILE_HACK
	KDOM_ENTER_SAFE

	switch(token)
	{
		case LSInputConstants::CharacterStream:
		{
			QTextIStream *s = characterStream();
			if(s)
				return getDOMString(s->read());

			return Undefined();
		}
		case LSInputConstants::ByteStream:
			return getDOMString(byteStream());
		case LSInputConstants::StringData:
			return getDOMString(stringData());
		case LSInputConstants::SystemId:
			return getDOMString(systemId());
		case LSInputConstants::Encoding:
			return getDOMString(encoding());
		default:
			kdWarning() << "Unhandled token in " << k_funcinfo << " : " << token << endl;
	}

	KDOM_LEAVE_SAFE(LSException)
#endif
	return Undefined();
}

void LSInput::putValueProperty(ExecState *exec, int token, ValueImp *value, int)
{
	KDOM_ENTER_SAFE

	switch(token)
	{
		case LSInputConstants::CharacterStream:
		{
			QString *s = new QString(toDOMString(exec, value).string());
			setCharacterStream(new QTextIStream(s));
			break;
		}
		case LSInputConstants::ByteStream:
			setByteStream(toDOMString(exec, value));
			break;
		case LSInputConstants::StringData:
			setStringData(toDOMString(exec, value));
			break;
		case LSInputConstants::SystemId:
			setSystemId(toDOMString(exec, value));
			break;
		case LSInputConstants::Encoding:
			setEncoding(toDOMString(exec, value));
			break;
		default:
			kdWarning() << "Unhandled token in " << k_funcinfo << " : " << token << endl;
	}

	KDOM_LEAVE_SAFE(LSException)
}

// The qdom way...
#define impl (static_cast<LSInputImpl *>(d))

LSInput LSInput::null;

LSInput::LSInput() : d(0)
{
}

LSInput::LSInput(LSInputImpl *i) : d(i)
{
	if(d)
		d->ref();
}

LSInput::LSInput(const LSInput &other) : d(0)
{
	(*this) = other;
}

KDOM_IMPL_DTOR_ASSIGN_OP(LSInput)

QTextIStream *LSInput::characterStream() const
{
	if(!d)
		return 0;

	return d->characterStream();
}

void LSInput::setCharacterStream(QTextIStream *characterStream)
{
	if(d)
		d->setCharacterStream(characterStream);
}

DOMString LSInput::byteStream() const
{
	if(!d)
		return DOMString();

	return DOMString(d->byteStream());
}

void LSInput::setByteStream(const DOMString &byteStream)
{
	if(d)
		d->setByteStream(byteStream);
}

DOMString LSInput::stringData() const
{
	if(!d)
		return DOMString();

	return DOMString(d->stringData());
}

void LSInput::setStringData(const DOMString &stringData)
{
	if(d)
		d->setStringData(stringData);
}

DOMString LSInput::systemId() const
{
	if(!d)
		return DOMString();

	return DOMString(d->systemId());
}

void LSInput::setSystemId(const DOMString &systemId)
{
	if(d)
		d->setSystemId(systemId);
}

DOMString LSInput::publicId() const
{
	if(!d)
		return DOMString();

	return DOMString(d->publicId());
}

void LSInput::setPublicId(const DOMString &publicId)
{
	if(d)
		d->setPublicId(publicId);
}

DOMString LSInput::baseURI() const
{
	if(!d)
		return DOMString();

	return DOMString(d->baseURI());
}

void LSInput::setBaseURI(const DOMString &baseURI)
{
	if(d)
		d->setBaseURI(baseURI);
}

DOMString LSInput::encoding() const
{
	if(!d)
		return DOMString();

	return DOMString(d->encoding());
}

void LSInput::setEncoding(const DOMString &encoding)
{
	if(d)
		d->setEncoding(encoding);
}

bool LSInput::certifiedText() const
{
	if(!d)
		return false;

	return d->certifiedText();
}

void LSInput::setCertifiedText(bool certifiedText)
{
	if(d)
		d->setCertifiedText(certifiedText);
}

// vim:ts=4:noet
