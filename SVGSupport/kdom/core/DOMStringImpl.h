/*
    Copyright (C) 2004, 2005 Nikolas Zimmermann <wildfox@kde.org>
                  2004, 2005 Rob Buis <buis@kde.org>

    Additional copyright (KHTML code)
              (C) 1999 Lars Knoll <knoll@kde.org>
			  (C) 2003 Dirk Mueller (mueller@kde.org)

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

#ifndef KDOM_StringImpl_H
#define KDOM_StringImpl_H

#include <qstring.h>

#include <kdom/Shared.h>

namespace KDOM
{
	class DOMStringImpl : public Shared
	{
	public:
		DOMStringImpl();
		DOMStringImpl(const QChar *str, unsigned int len);
		DOMStringImpl(const QString &str);
		DOMStringImpl(const char *str);
		virtual ~DOMStringImpl();

		const QChar &operator[](int i) const;

		unsigned int length() const;
		void setLength(unsigned int len);

		void insert(DOMStringImpl *str, unsigned int pos);

		void append(const char *str);
		void append(const QString &str);
		void append(DOMStringImpl *str);

		bool isEmpty() const;
		bool containsOnlyWhitespace() const;

		void truncate(int len);
		void remove(unsigned int pos, int len = 1);

		DOMStringImpl *substring(unsigned int pos, unsigned int len);
		DOMStringImpl *split(unsigned int pos);

		DOMStringImpl *collapseWhiteSpace(bool preserveLF, bool preserveWS);

		DOMStringImpl *lower() const;
		DOMStringImpl *upper() const;
		DOMStringImpl *capitalize() const;

		QChar *unicode() const;
		QString string() const;

		int toInt(bool *ok = 0) const;

		DOMStringImpl *copy() const;

	private:
		QChar *m_str;
		unsigned int m_len;
	};
};

#endif

// vim:ts=4:noet
