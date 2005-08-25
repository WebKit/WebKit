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

#ifndef KDOM_DOMConfigurationImpl_H
#define KDOM_DOMConfigurationImpl_H

#include <kdom/Shared.h>
#include <kdom/DOMString.h>
#include <kdom/impl/DOMErrorHandlerImpl.h>

namespace KDOM
{
	class DOMStringImpl;
	class DOMUserDataImpl;
	class DOMStringListImpl;

	class DOMConfigurationImpl : public Shared
	{
	public:
		DOMConfigurationImpl();
		virtual ~DOMConfigurationImpl();

		void setParameter(DOMStringImpl *name, DOMUserDataImpl *value);
		void setParameter(DOMStringImpl *name, bool value);
		DOMUserDataImpl *getParameter(DOMStringImpl *name) const;
		bool canSetParameter(DOMStringImpl *name, DOMUserDataImpl *value) const;
		DOMStringListImpl *parameterNames() const;

		// Internal
		DOMErrorHandlerImpl *errHandler() const;
		bool getParameter(int flag) const;
		void setParameter(int flag, bool b);

		DOMStringImpl *normalizeCharacters(DOMStringImpl *data) const;

	private:
		DOMErrorHandlerImpl *m_errHandler;

		int m_flags;
		static DOMStringListImpl *m_paramNames;
	};

	// TODO: Rob created these and I feel bad about that (Niko)
	static DOMString CANONICAL_FORM("canonical-form");
	static DOMString CDATA_SECTIONS("cdata-sections");
	static DOMString CHECK_CHARACTER_NORMALIZATION("check-character-normalization");
	static DOMString COMMENTS("comments");
	static DOMString DATATYPE_NORMALIZATION("datatype-normalization");
	static DOMString ELEMENT_CONTENT_WHITESPACE("element-content-whitespace");
	static DOMString ENTITIES("entities");
	static DOMString ERROR_HANDLER("error-handler");
	static DOMString INFOSET("infoset");
	static DOMString NAMESPACES("namespaces");
	static DOMString NAMESPACE_DECLARATIONS("namespace-declarations");
	static DOMString NORMALIZE_CHARACTERS("normalize-characters");
	static DOMString SCHEMA_LOCATION("schema-location");
	static DOMString SCHEMA_TYPE("schema-type");
	static DOMString SPLIT_CDATA_SECTIONS("split-cdata-sections");
	static DOMString VALIDATE("validate");
	static DOMString VALIDATE_IF_SCHEMA("validate-if-schema");
	static DOMString WELL_FORMED("well-formed");

	// LSParser specific
	static DOMString CHARSET_OVERRIDES_XML_ENCODING("charset-overrides-xml-encoding");
	static DOMString DISALLOW_DOCTYPE("disallow-doctype");
	static DOMString IGNORE_UNKNOWN_CD("ignore-unknown-character-denormalizations");
	static DOMString RESOURCE_RESOLVER("resource-resolver");
	static DOMString SUPPORTED_MEDIA_TYPE_ONLY("supported-media-types-only");

	// LSSerializer specific
	static DOMString DISCARD_DEFAULT_CONTENT("discard-default-content");
	static DOMString FORMAT_PRETTY_PRINT("format-pretty-print");
	static DOMString XML_DECLARATION("xml-declaration");
};

#endif

// vim:ts=4:noet
