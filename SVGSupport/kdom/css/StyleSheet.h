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

#ifndef KDOM_StyleSheet_H
#define KDOM_StyleSheet_H

#include <kdom/kdom.h>
#include <kdom/ecma/DOMLookup.h>

namespace KDOM
{
	class Node;
	class MediaList;
	class StyleSheetImpl;
	class StyleSheet 
	{
	public:
		StyleSheet();
		explicit StyleSheet(StyleSheetImpl *i);
		StyleSheet(const StyleSheet &other);
		virtual ~StyleSheet();

		// Operators
		StyleSheet &operator=(const StyleSheet &other);
		bool operator==(const StyleSheet &other) const;
		bool operator!=(const StyleSheet &other) const;

		// 'StyleSheet' functions
		virtual DOMString type() const;
		void setDisabled(bool disabled);
		bool disabled() const;

		Node ownerNode() const;
		StyleSheet parentStyleSheet() const;
		DOMString href() const;
		DOMString title() const;
		MediaList media() const;

		// Internal
		KDOM_INTERNAL_BASE(StyleSheet)

		bool isCSSStyleSheet() const;

	protected:
		StyleSheetImpl *d;

	public: // EcmaScript section
		KDOM_BASECLASS_GET
		KDOM_PUT

		KJS::ValueImp *getValueProperty(KJS::ExecState *exec, int token) const;
		void putValueProperty(KJS::ExecState *exec, int token, KJS::ValueImp *value, int attr);		
	};
};

#endif

// vim:ts=4:noet
