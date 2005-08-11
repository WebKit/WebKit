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

#include "DOMString.h"
#include "DOMStringImpl.h"

using namespace KDOM;

DOMString::DOMString() : d(0)
{
}

DOMString::DOMString(DOMStringImpl *i) : d(i)
{
	if(d)	
		d->ref();
}

DOMString::DOMString(const DOMString &other) : d(0)
{
	(*this) = other;
}

DOMString::DOMString(const QChar *string, unsigned int length) : d(0)
{
	d = new DOMStringImpl(string, length);
	d->ref();	
}

DOMString::DOMString(const QString &string) : d(0)
{
	if(!string.isNull())
	{
		d = new DOMStringImpl(string.unicode(), string.length());
		d->ref();
	}
}

DOMString::DOMString(const char *string) : d(0)
{
	d = new DOMStringImpl(string);
	d->ref();
}

DOMString::~DOMString()
{
	if(d)
		d->deref();
}

DOMString &DOMString::operator=(const DOMString &other)
{
	KDOM_SAFE_SET(d, other.d);	
	return *this;
}

DOMString &DOMString::operator+=(const DOMString &other)
{
	if(!d)
	{
		d = other.d;

		if(d)
			d->ref();

		return *this;
	}
	
	if(other.d)
	{
		DOMStringImpl *i = d->copy();
		d->deref();
		d = i;
		d->ref();
		d->append(other.d);
	}
	
	return *this;
}

DOMString DOMString::operator+(const DOMString &other) const
{
	if(!d)
		return other.copy();
		
	if(other.d)
	{
		DOMString s = copy();
		s += other;
		return s;
	}

	return copy();
}

const QChar &DOMString::operator[](unsigned int i) const
{
    static const QChar nullChar = 0;

    if(!d || i >= d->length())
		return nullChar;

    return *(d->unicode() + i);
}

unsigned int DOMString::length() const
{
	if(!d)
		return 0;

	return d->length();
}

void DOMString::insert(DOMString str, unsigned int position)
{
	if(!d)
	{
		d = str.d->copy();
		d->ref();
	}
	else
		d->insert(str.d, position);
}

int DOMString::find(const QChar c, int start) const
{
	unsigned int l = start;
	if(!d || l >= d->length())
		return -1;

	while(l < d->length())
	{
		if(*(d->unicode() + l) == c)
			return l;

		l++;
	}

	return -1;
}

void DOMString::truncate(unsigned int length)
{
	if(d)
		d->truncate(length);
}

void DOMString::remove(unsigned int position, int length)
{
	if(d)
		d->remove(position, length);
}

DOMString DOMString::substring(unsigned int position, unsigned int length)
{
	if(!d)
		return DOMString();

	return DOMString(d->substring(position, length));
}

DOMString DOMString::split(unsigned int position)
{
	if(!d)
	 	return DOMString();

	return DOMString(d->split(position));
}

DOMString DOMString::lower() const
{
	if(!d)
		return DOMString();

	return DOMString(d->lower());
}

DOMString DOMString::upper() const
{
	if(!d)
		return DOMString();

	return DOMString(d->upper());
}

QChar *DOMString::unicode() const
{
	if(!d)
		return 0;

	return d->unicode();
}

QString DOMString::string() const
{
	if(!d || !d->unicode() || !d->length())
		return QString();

	return QString(d->unicode(), d->length());
}

int DOMString::toInt(bool *ok) const
{
	if(!d)
		return -1;

	return d->toInt(ok);
}

DOMString DOMString::copy() const
{
	if(!d)
		return DOMString();

	return DOMString(d->copy());
}

bool DOMString::isNull() const
{
    return !d;
}

bool DOMString::isEmpty() const
{
    return (isNull() || !d->length());
}

DOMStringImpl *DOMString::implementation() const
{
	return d;
}

// Comparision functions
bool KDOM::strcasecmp(const DOMString &as, const DOMString &bs)
{
	if(as.length() != bs.length())
		return true;

	const QChar *a = as.unicode();
	const QChar *b = bs.unicode();

	if(a == b)
		return false;

	if(!(a && b))
		return true;

	int l = as.length();
	while(l--)
	{
		if(*a != *b && a->lower() != b->lower())
			return true;

		a++;
		b++;
	}

	return false;
}

bool KDOM::strcasecmp(const DOMString &as, const char *bs)
{
	const QChar *a = as.unicode();
	int l = as.length();
	if(!bs)
		return (l != 0);

	while(l--)
	{
		if(a->latin1() != *bs)
		{
			char cc = ((*bs >= 'A') && (*bs <= 'Z')) ? ((*bs) + 'a' - 'A') : (*bs);
			if(a->lower().latin1() != cc)
				return true;
        	}	
        
		a++;
		bs++;
	}

	return (*bs != '\0');
}

bool KDOM::operator==(const DOMString &a, const DOMString &b)
{
	// Special case, null strings
	if(a.isNull() && b.isNull())
		return true;

	unsigned int l = a.length();

	if(l != b.length())
		return false;

	if(!memcmp(a.unicode(), b.unicode(), l * sizeof(QChar)))
		return true;

	return false;
}

bool KDOM::operator==(const DOMString &a, const QString &b)
{
	unsigned int l = a.length();

	if(l != b.length())
		return false;

	if(!memcmp(a.unicode(), b.unicode(), l * sizeof(QChar)))
		return true;
	
	return false;
}

bool KDOM::operator==(const DOMString &a, const char *b)
{
	DOMStringImpl *aimpl = a.implementation();
	if(!b)
		return !aimpl;

	if(aimpl)
	{
		int alen = aimpl->length();
		const QChar *aptr = aimpl->unicode();
		while(alen--)
		{
			unsigned char c = *b++;
			if(!c || (*aptr++).unicode() != c)
				return false;
		}
	}

	return !*b;
}

// vim:ts=4:noet
