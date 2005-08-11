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

#ifndef KDOM_DOMConfiguration_H
#define KDOM_DOMConfiguration_H

#include <kdom/ecma/DOMLookup.h>
#include <kdom/DOMString.h>

namespace KDOM
{
	enum
	{
		FEATURE_CANONICAL_FORM			= 0x000001,
		FEATURE_CDATA_SECTIONS			= 0x000002,
		FEATURE_COMMENTS			= 0x000004,
		FEATURE_DATATYPE_NORMALIZATION		= 0x000008,
		FEATURE_ENTITIES			= 0x000010,
		FEATURE_WELL_FORMED			= 0x000020,
		FEATURE_NAMESPACES			= 0x000040,
		FEATURE_NAMESPACE_DECLARATIONS		= 0x000080,
		FEATURE_NORMALIZE_CHARACTERS		= 0x000100,
		FEATURE_SPLIT_CDATA_SECTIONS		= 0x000200,
		FEATURE_VALIDATE			= 0x000400,
		FEATURE_VALIDATE_IF_SCHEMA		= 0x000800,
		FEATURE_WHITESPACE_IN_ELEMENT_CONTENT	= 0x001000,
		FEATURE_CHECK_CHARACTER_NORMALIZATION	= 0x002000,
		// LSParser specific
		FEATURE_CHARSET_OVERRIDES_XML_ENCODING	= 0x004000,
		FEATURE_DISALLOW_DOCTYPE		= 0x008000,
		FEATURE_IGNORE_UNKNOWN_CD		= 0x010000,
		FEATURE_SUPPORTED_MEDIA_TYPE_ONLY	= 0x020000,
		// LSSerializer specific
		FEATURE_DISCARD_DEFAULT_CONTENT		= 0x040000,
		FEATURE_FORMAT_PRETTY_PRINT		= 0x080000,
		FEATURE_XML_DECLARATION			= 0x100000
	};

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

	class DOMUserData;
	class DOMStringList;
	class DOMConfigurationImpl;

	/**
	 * @short The DOMConfiguration interface represents the configuration of
	 * a document and maintains a table of recognized parameters.
	 *
	 * @author Nikolas Zimmermann <wildfox@kde.org>
	 * @author Rob Buis <buis@kde.org>
	 */
	// Introduced in DOM Level 3:
	class DOMConfiguration
	{
	public:
		DOMConfiguration();
		explicit DOMConfiguration(DOMConfigurationImpl *i);
		DOMConfiguration(const DOMConfiguration &other);
		virtual ~DOMConfiguration();

		// Operators
		DOMConfiguration &operator=(const DOMConfiguration &other);
		bool operator==(const DOMConfiguration &other) const;
		bool operator!=(const DOMConfiguration &other) const;

		// 'DOMConfiguration' functions
		/**
		 * Set the value of a parameter.
		 *
		 * @param name The name of the parameter.
		 * @param value The new value or null if the user wishes to unset
		 * the parameter. While the type of the value parameter is defined
		 * as DOMUserData, the object type must match the type defined by the
		 * definition of the parameter. For example, if the parameter is
		 * "error-handler", the value must be of type DOMErrorHandler. 
		 */
		void setParameter(const DOMString &name, DOMUserData value);
		void setParameter(const DOMString &name, bool value);

		/**
		 * Return the value of a parameter if known.
		 *
		 * @param name The name of the parameter.
		 *
		 * @returns The current object associated with the specified parameter or
		 * null if no object has been associated or if the parameter is not supported. 
		 */
		DOMUserData getParameter(const DOMString &name) const;

		/**
		 * Check if setting a parameter to a specific value is supported.
		 *
		 * @param name The name of the parameter to check.
		 * @param value An object. if null, the returned value is true.
		 *
		 * @returns true if the parameter could be successfully set to the
		 * specified value, or false if the parameter is not recognized or the
		 * requested value is not supported. This does not change the current
		 * value of the parameter itself.
		 */
		bool canSetParameter(const DOMString &name, DOMUserData value) const;

		/**
		 * The list of the parameters supported by this DOMConfiguration object
		 * and for which at least one value can be set by the application.
		 * Note that this list can also contain parameter names defined outside
		 * this specification.
		 */
		DOMStringList parameterNames() const;

		// Internal
		KDOM_INTERNAL_BASE(DOMConfiguration)

	protected:
		DOMConfigurationImpl *d;

	public: // EcmaScript section
		KDOM_BASECLASS_GET

		KJS::ValueImp *getValueProperty(KJS::ExecState *exec, int token) const;
	};
};

KDOM_DEFINE_PROTOTYPE(DOMConfigurationProto)
KDOM_IMPLEMENT_PROTOFUNC(DOMConfigurationProtoFunc, DOMConfiguration)

#endif

// vim:ts=4:noet
