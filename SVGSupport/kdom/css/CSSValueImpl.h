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

#ifndef KDOM_CSSValueImpl_H
#define KDOM_CSSValueImpl_H

#include <qstringlist.h>

#include <kdom/DOMString.h>
#include <kdom/css/kdomcss.h>
#include <kdom/css/impl/StyleBaseImpl.h>

namespace KDOM
{
	class CSSValueImpl : public StyleBaseImpl
	{
	public:
		CSSValueImpl();
		virtual ~CSSValueImpl();

		// 'CSSValueImpl' functions
		virtual void setCssText(const DOMString &cssText);
		virtual DOMString cssText() const = 0;

		virtual unsigned short cssValueType() const;

		virtual bool isValue() const { return true; }
		virtual bool isFontValue() const { return false; }
	};

	class CSSInheritedValueImpl : public CSSValueImpl
	{
	public:
		virtual unsigned short cssValueType() const;
		virtual DOMString cssText() const;
	};

	class CSSInitialValueImpl : public CSSValueImpl
	{
	public:
		virtual unsigned short cssValueType() const;
		virtual DOMString cssText() const;
	};

	class CSSValueListImpl;
	class CSSPrimitiveValueImpl;

	class FontValueImpl : public CSSValueImpl
	{
	public:
		FontValueImpl();
		virtual ~FontValueImpl();

		virtual unsigned short cssValueType() const;
		virtual DOMString cssText() const;

		virtual bool isFontValue() const { return true; }

		CSSPrimitiveValueImpl *style;
		CSSPrimitiveValueImpl *variant;
		CSSPrimitiveValueImpl *weight;
		CSSPrimitiveValueImpl *size;
		CSSPrimitiveValueImpl *lineHeight;
		CSSValueListImpl *family;
	};

	// Used for quotes
	class QuotesValueImpl : public CSSValueImpl
	{
	public:
    	QuotesValueImpl();
		virtual ~QuotesValueImpl();

		virtual unsigned short cssValueType() const;
		virtual DOMString cssText() const;

		void addLevel(const QString &open, const QString &close);
		QString openQuote(int level) const;
		QString closeQuote(int level) const;

		unsigned int levels;
		QStringList data;
	};
	
	// Used for text-shadow and box-shadow
	class ShadowValueImpl : public CSSValueImpl
	{
	public:
    	ShadowValueImpl(CSSPrimitiveValueImpl *_x, CSSPrimitiveValueImpl *_y,
						CSSPrimitiveValueImpl *_blur, CSSPrimitiveValueImpl *_color);
		virtual ~ShadowValueImpl();

		virtual unsigned short cssValueType() const;
		virtual DOMString cssText() const;

		CSSPrimitiveValueImpl *x;
		CSSPrimitiveValueImpl *y;
		CSSPrimitiveValueImpl *blur;
		CSSPrimitiveValueImpl *color;
	};

	// Used for counter-reset and counter-increment
	class CounterActImpl : public CSSValueImpl
	{
	public:
		CounterActImpl(DOMString &c, short v);
		virtual ~CounterActImpl();

		virtual unsigned short cssValueType() const;
		virtual DOMString cssText() const;

		DOMString counter() const;

		short value() const;
		void setValue(const short v);

		DOMString m_counter;
		short m_value;
	};
};

#endif

// vim:ts=4:noet
