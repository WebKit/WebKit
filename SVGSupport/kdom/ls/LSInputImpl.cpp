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

#include <qtextstream.h>

#include "DOMString.h"
#include "LSInputImpl.h"
#include "DOMStringImpl.h"

using namespace KDOM;

LSInputImpl::LSInputImpl()
: Shared(true), m_characterStream(0), m_byteStream(0), m_stringData(0), m_systemId(0),
  m_publicId(0), m_baseURI(0), m_encoding(0), m_certifiedText(false)
{
}

LSInputImpl::~LSInputImpl()
{
	delete m_characterStream;

	if(m_byteStream)
		m_byteStream->deref();
	if(m_stringData)
		m_stringData->deref();
	if(m_systemId)
		m_systemId->deref();
	if(m_publicId)
		m_publicId->deref();
	if(m_baseURI)
		m_baseURI->deref();
	if(m_encoding)
		m_encoding->deref();
}

QTextIStream *LSInputImpl::characterStream() const
{
	return m_characterStream;
}

void LSInputImpl::setCharacterStream(QTextIStream *characterStream)
{
	delete m_characterStream;
	m_characterStream = characterStream;
}

DOMString LSInputImpl::byteStream() const
{
	return DOMString(m_byteStream);
}

void LSInputImpl::setByteStream(const DOMString &byteStream)
{
	if(m_byteStream)
		m_byteStream->deref();

	m_byteStream = byteStream.implementation();

	if(m_byteStream)
		m_byteStream->ref();
}

DOMString LSInputImpl::stringData() const
{
	return DOMString(m_stringData);
}

void LSInputImpl::setStringData(const DOMString &stringData)
{
	if(m_stringData)
		m_stringData->deref();

	m_stringData = stringData.implementation();

	if(m_stringData)
		m_stringData->ref();
}

DOMString LSInputImpl::systemId() const
{
	return DOMString(m_systemId);
}

void LSInputImpl::setSystemId(const DOMString &systemId)
{
	if(m_systemId)
		m_systemId->deref();

	m_systemId = systemId.implementation();

	if(m_systemId)
		m_systemId->ref();
}

DOMString LSInputImpl::publicId() const
{
	return DOMString(m_publicId);
}

void LSInputImpl::setPublicId(const DOMString &publicId)
{
	if(m_publicId)
		m_publicId->deref();

	m_publicId = publicId.implementation();

	if(m_publicId)
		m_publicId->ref();
}

DOMString LSInputImpl::baseURI() const
{
	return DOMString(m_baseURI);
}

void LSInputImpl::setBaseURI(const DOMString &baseURI)
{
	if(m_baseURI)
		m_baseURI->deref();

	m_baseURI = baseURI.implementation();

	if(m_baseURI)
		m_baseURI->ref();
}

DOMString LSInputImpl::encoding() const
{
	return DOMString(m_encoding);
}

void LSInputImpl::setEncoding(const DOMString &encoding)
{
	if(m_encoding)
		m_encoding->deref();

	m_encoding = encoding.implementation();

	if(m_encoding)
		m_encoding->ref();
}

bool LSInputImpl::certifiedText() const
{
	return m_certifiedText;
}

void LSInputImpl::setCertifiedText(bool certifiedText)
{
	m_certifiedText = certifiedText;
}

// vim:ts=4:noet
