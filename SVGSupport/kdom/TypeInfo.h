/*
    Copyright (C) 2005 Frans Englich <frans.englich@telia.com>

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

#ifndef KDOM_TypeInfo_H
#define KDOM_TypeInfo_H

#include <kdom/ecma/DOMLookup.h>

namespace KDOM
{
	class DOMString;
	class TypeInfoImpl;

	// Introduced in DOM Level 3:
	class TypeInfo
	{
	public:
		TypeInfo();
		explicit TypeInfo(TypeInfoImpl *i);
		TypeInfo(const TypeInfo &other);
		virtual ~TypeInfo();

		// Operators
		TypeInfo &operator=(const TypeInfo &other);
		bool operator==(const TypeInfo &other) const;
		bool operator!=(const TypeInfo &other) const;

		bool isDerivedFrom(const DOMString &typeNamespaceArg,
						   const DOMString &typeNameArg,
						   unsigned long derivationMethod) const;

		DOMString typeName() const;
		DOMString typeNamespace() const;

		// Internal
		KDOM_INTERNAL_BASE(TypeInfo)

	protected:
		TypeInfoImpl *d;

	public: // EcmaScript section
		KDOM_BASECLASS_GET

		KJS::Value getValueProperty(KJS::ExecState *exec, int token) const;
	};

};

KDOM_DEFINE_PROTOTYPE(TypeInfoProto)
KDOM_IMPLEMENT_PROTOFUNC(TypeInfoProtoFunc, TypeInfo)

#endif

// vim:ts=4:noet
