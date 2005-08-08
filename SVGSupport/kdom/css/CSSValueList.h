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

#ifndef KDOM_CSSValueList_H
#define KDOM_CSSValueList_H

#include <kdom/css/CSSValue.h>

namespace KDOM
{
	class CSSValueListImpl;
	class CSSValueList : public CSSValue
	{
	public:
		CSSValueList();
		explicit CSSValueList(CSSValueListImpl *i);
		CSSValueList(const CSSValueList &other);
		CSSValueList(const CSSValue &other);
		virtual ~CSSValueList();

		// Operators
		CSSValueList &operator=(const CSSValueList &other);
		CSSValueList &operator=(const CSSValue &other);

		// 'CSSValueList' functions
		unsigned long length() const;
		CSSValue item(unsigned long index) const;

		// Internal
		KDOM_INTERNAL(CSSValueList)

	public: // EcmaScript section
		KDOM_GET
		KDOM_PUT

		KJS::ValueImp *getValueProperty(KJS::ExecState *exec, int token) const;
	};
};

KDOM_DEFINE_PROTOTYPE(CSSValueListProto)
KDOM_IMPLEMENT_PROTOFUNC(CSSValueListProtoFunc, CSSValueList)

#endif

// vim:ts=4:noet
