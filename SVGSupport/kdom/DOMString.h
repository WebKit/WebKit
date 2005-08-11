/*
    Copyright (C) 2004, 2005 Nikolas Zimmermann <wildfox@kde.org>
                  2004, 2005 Rob Buis <buis@kde.org>

    Additional copyright (KHTML code)
              (C) 1999 Lars Knoll <knoll@kde.org>

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

#ifndef KDOM_String_H
#define KDOM_String_H

#include <qstring.h>

namespace KDOM
{
	class DOMStringImpl;
	class DOMString
	{
	public:
		DOMString();
		explicit DOMString(DOMStringImpl *i);
		DOMString(const DOMString &other);
		DOMString(const QChar *string, unsigned int length);
		DOMString(const QString &string);
		DOMString(const char *string);
		virtual ~DOMString();

		DOMString &operator=(const DOMString &other);
		DOMString &operator+=(const DOMString &other);
		DOMString operator+(const DOMString &other) const;

		/**
		 * The character at position i of the DOMString.
		 * If i >= length(), the character returned will be 0.
		 */
		const QChar &operator[](unsigned int i) const;

		unsigned int length() const;

		void insert(DOMString string, unsigned int position);
		int find(const QChar c, int start = 0) const;

		void truncate(unsigned int length);
		void remove(unsigned int position, int length = 1);

		/**
		 * Returns a sub-portion of this string
		 */
		DOMString substring(unsigned int position, unsigned int length);

		/**
		 * Splits the string into two. The original string
		 * gets truncated to pos, and the rest is returned.
		 */
		DOMString split(unsigned int position);

		/**
		 * Returns a lowercase version of the string
		 */
		DOMString lower() const;

		/**
		 * Returns an uppercase version of the string
		 */
		DOMString upper() const;

		QChar *unicode() const;
		QString string() const;

		int toInt(bool *ok = 0) const;

		DOMString copy() const;

		bool isNull() const;
		bool isEmpty() const;

		// Internal
		DOMStringImpl *implementation() const;

	protected:
		DOMStringImpl *d;
	};

	bool operator==(const DOMString &a, const DOMString &b);
	bool operator==(const DOMString &a, const QString &b);
	bool operator==(const DOMString &a, const char *b);

	inline bool operator!=(const DOMString &a, const DOMString &b) { return !(a == b); }
	inline bool operator!=(const DOMString &a, const QString &b) { return !(a == b); }
	inline bool operator!=(const DOMString &a, const char *b) { return !(a == b); }
	inline bool strcmp(const DOMString &a, const DOMString &b) { return a != b; }

	// returns false when equal, true otherwise (ignoring case)
	bool strcasecmp(const DOMString &a, const DOMString &b);
	bool strcasecmp(const DOMString &a, const char *b);
};

#endif

// vim:ts=4:noet
