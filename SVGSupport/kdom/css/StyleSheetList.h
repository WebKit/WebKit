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

#ifndef KDOM_StyleSheetList_H
#define KDOM_StyleSheetList_H

#include <kdom/ecma/DOMLookup.h>

namespace KDOM
{
	class StyleSheet;
	class StyleSheetListImpl;
	class StyleSheetList
	{
	public:
		StyleSheetList();
		explicit StyleSheetList(StyleSheetListImpl *i);
		StyleSheetList(const StyleSheetList &other);
		virtual ~StyleSheetList();

		// Operators
		StyleSheetList &operator=(const StyleSheetList &other);
		bool operator==(const StyleSheetList &other) const;
		bool operator!=(const StyleSheetList &other) const;

		// 'StyleSheetList' functions
		unsigned long length() const;
		StyleSheet item(unsigned long index) const;

		// Internal
		KDOM_INTERNAL_BASE(StyleSheetList)

	protected:
		StyleSheetListImpl *d;

	public: // EcmaScript section
		KDOM_GET

		KJS::ValueImp *getValueProperty(KJS::ExecState *exec, int token) const;
	};
};

KDOM_DEFINE_PROTOTYPE(StyleSheetListProto)
KDOM_IMPLEMENT_PROTOFUNC(StyleSheetListProtoFunc, StyleSheetList)

#endif

// vim:ts=4:noet
