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

#include "Node.h"
#include "Document.h"
#include "DOMString.h"
#include "DOMLookup.h"
#include "DOMException.h"
#include "DOMObject.h"
#include "DocumentType.h"
#include "CSSStyleSheet.h"
#include "DOMExceptionImpl.h"
#include "DOMImplementation.h"
#include "DOMImplementationImpl.h"
#include "LSParser.h"
#include "LSSerializer.h"
#include "LSInput.h"
#include "LSOutput.h"

#include "DOMConstants.h"
#include "DOMImplementation.lut.h"
using namespace KDOM;
using namespace KJS;

/*
@begin DOMImplementation::s_hashTable 2
 dummy					DOMImplementationConstants::Dummy				DontDelete|ReadOnly
@end
@begin DOMImplementationProto::s_hashTable 11
 hasFeature				DOMImplementationConstants::HasFeature			DontDelete|Function 2
 createDocumentType		DOMImplementationConstants::CreateDocumentType	DontDelete|Function 3
 createDocument			DOMImplementationConstants::CreateDocument		DontDelete|Function 3
 getFeature				DOMImplementationConstants::GetFeature			DontDelete|Function 2
 createCSSStyleSheet	DOMImplementationConstants::CreateCSSStyleSheet	DontDelete|Function 2
 createLSParser			DOMImplementationConstants::CreateLSParser		DontDelete|Function 2
 createLSSerializer		DOMImplementationConstants::CreateLSSerializer	DontDelete|Function 0
 createLSInput			DOMImplementationConstants::CreateLSInput		DontDelete|Function 0
 createLSOutput			DOMImplementationConstants::CreateLSOutput		DontDelete|Function 0
@end
*/

KDOM_IMPLEMENT_PROTOTYPE("DOMImplementation", DOMImplementationProto, DOMImplementationProtoFunc)

Value DOMImplementation::getValueProperty(ExecState *exec, int token) const
{
	KDOM_ENTER_SAFE

	switch(token)
	{
		default:
			kdWarning() << "Unhandled token in " << k_funcinfo << " : " << token << endl;
	}

	KDOM_LEAVE_SAFE(DOMException)
	return Undefined();
}

Value DOMImplementationProtoFunc::call(ExecState *exec, Object &thisObj, const List &args)
{
	KDOM_CHECK_THIS(DOMImplementation)
	KDOM_ENTER_SAFE

	switch(id)
	{
		case DOMImplementationConstants::HasFeature:
		{
			DOMString feature = toDOMString(exec, args[0]);
			DOMString version = toDOMString(exec, args[1]);
			return KJS::Boolean(obj.hasFeature(feature, version));
		}
		case DOMImplementationConstants::CreateDocumentType:
		{
			DOMString qualifiedName = toDOMString(exec, args[0]);
			DOMString publicId = toDOMString(exec, args[1]);
			DOMString systemId = toDOMString(exec, args[2]);

			DocumentType documentType = obj.createDocumentType(qualifiedName, publicId, systemId);
			return getDOMNode(exec, documentType);
		}
		case DOMImplementationConstants::CreateDocument:
		{
			DOMString namespaceURI = toDOMString(exec, args[0]);
			DOMString qualifiedName = toDOMString(exec, args[1]);

			DocumentType documentType = ecma_cast<DocumentType>(exec, args[2], &toDocumentType);
			return getDOMNode(exec, obj.createDocument(namespaceURI, qualifiedName, documentType));
		}
		case DOMImplementationConstants::GetFeature:
		{
			DOMString feature = toDOMString(exec, args[0]);
			DOMString version = toDOMString(exec, args[1]);
			DOMObject object = obj.getFeature(feature, version);
			DOMImplementationImpl *imp = static_cast<DOMImplementationImpl *>(object.handle());
			if(imp)
				return DOMImplementation(imp).cache(exec);

			return Undefined();
		}
		case DOMImplementationConstants::CreateCSSStyleSheet:
		{
			DOMString title = toDOMString(exec, args[0]);
			DOMString media = toDOMString(exec, args[1]);
			return safe_cache<CSSStyleSheet>(exec, obj.createCSSStyleSheet(title, media));
		}
		case DOMImplementationConstants::CreateLSParser:
		{
			unsigned short mode = args[0].toUInt32(exec);
			DOMString schemaType = toDOMString(exec, args[1]);
			return safe_cache<LSParser>(exec, obj.createLSParser(mode, schemaType));
		}
		case DOMImplementationConstants::CreateLSSerializer:
			return safe_cache<LSSerializer>(exec, obj.createLSSerializer());
		case DOMImplementationConstants::CreateLSInput:
			return safe_cache<LSInput>(exec, obj.createLSInput());
		case DOMImplementationConstants::CreateLSOutput:
			return safe_cache<LSOutput>(exec, obj.createLSOutput());
		default:
			kdWarning() << "Unhandled function id in " << k_funcinfo << " : " << id << endl;
	}

	KDOM_LEAVE_CALL_SAFE(DOMException)
	return Undefined();
}

DOMImplementation DOMImplementation::null;

DOMImplementation::DOMImplementation()
: DOMImplementationCSS(), DOMImplementationLS(), d(0)
{
}

DOMImplementation::DOMImplementation(DOMImplementationImpl *i) : d(i)
{
}

DOMImplementation::DOMImplementation(const DOMImplementation &other)
: DOMImplementationCSS(), DOMImplementationLS(), d(0)
{
	(*this) = other;
}

DOMImplementation::~DOMImplementation()
{
}

DOMImplementation &DOMImplementation::operator=(const DOMImplementation &other)
{
	d = other.d;
	return *this;
}

bool DOMImplementation::operator==(const DOMImplementation &other) const
{
	return d == other.d;
}

bool DOMImplementation::operator!=(const DOMImplementation &other) const
{
	return !operator==(other);
}

CSSStyleSheet DOMImplementation::createCSSStyleSheet(const DOMString &title, const DOMString &media) const
{
	if(!d)
		return CSSStyleSheet::null;

	return CSSStyleSheet(d->createCSSStyleSheet(title, media));
}

LSParser DOMImplementation::createLSParser(unsigned short mode, const DOMString &schemaType) const
{
	if(!d)
		return LSParser::null;

	return LSParser(d->createLSParser(mode, schemaType));
}

LSSerializer DOMImplementation::createLSSerializer() const
{
	if(!d)
		return LSSerializer::null;

	return LSSerializer(d->createLSSerializer());
}

LSInput DOMImplementation::createLSInput() const
{
	if(!d)
		return LSInput::null;

	return LSInput(d->createLSInput());
}

LSOutput DOMImplementation::createLSOutput() const
{
	if(!d)
		return LSOutput::null;

	return LSOutput(d->createLSOutput());
}

bool DOMImplementation::hasFeature(const DOMString &feature, const DOMString &version)
{
	if(!d)
		return false;

	return d->hasFeature(feature, version);
}

DocumentType DOMImplementation::createDocumentType(const DOMString &qualifiedName, const DOMString &publicId, const DOMString &systemId) const 
{
	if(!d)
		return DocumentType::null;

	return DocumentType(d->createDocumentType(qualifiedName, publicId, systemId));
}

Document DOMImplementation::createDocument(const DOMString &namespaceURI, const DOMString &qualifiedName, const DocumentType &doctype) const 
{
	if(!d)
		return Document::null;

	return Document(d->createDocument(namespaceURI, qualifiedName, doctype, true, 0 /* no view */));
}

DOMObject DOMImplementation::getFeature(const DOMString &feature, const DOMString &version) const
{
	if(d && d->hasFeature(feature, version))
		return DOMObject(d);

	return DOMObject::null;
}

DOMImplementation DOMImplementation::self() 
{
	return DOMImplementation(DOMImplementationImpl::self());
}

// vim:ts=4:noet
