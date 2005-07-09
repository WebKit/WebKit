/*
    Copyright (C) 2004 Richard Moore <rich@kde.org>
    Copyright (C) 2004 Zack Rusin <zack@kde.org>
    Copyright (C) 2005 Frans Englich <frans.englich@telia.com>

    This file is part of the KDE project

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Library General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or(at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Library General Public License for more details.

    You should have received a copy of the GNU Library General Public License
    along with this library; see the file COPYING.LIB.  If not, write to
    the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
    Boston, MA 02111-1307, USA.
*/

#include <float.h>

#include "DOMString.h"

#include "BooleanImpl.h"
#include "NumberImpl.h"
#include "StringImpl.h"

#ifndef APPLE_CHANGES
#include <math.h>
#else
#include <cmath>
using namespace std;
#endif

using namespace KDOM;
using namespace KDOM::XPath;

NumberImpl::NumberImpl()
					   : ValueImpl(ValueImpl::ValueNumber),
					   m_value(0)
{
}

NumberImpl::NumberImpl(uint value)
					   : ValueImpl(ValueImpl::ValueNumber),
					   m_value(value)
{
}

NumberImpl::NumberImpl(int value)
					   : ValueImpl(ValueImpl::ValueNumber),
					   m_value(value)
{
}

NumberImpl::NumberImpl(double value)
					   : ValueImpl(ValueImpl::ValueNumber),
					   m_value(value)
{
}

NumberImpl::~NumberImpl()
{
}

int NumberImpl::intValue() const
{
	return(int) m_value;
}

StringImpl *NumberImpl::toString()
{
	DOMString s;
	
	if(finite(m_value))
	{
		double intval = round(m_value);
		double diff = fabs(m_value-intval);
	
		if(diff < DBL_EPSILON)
			s = DOMString(QString::number((int) intval));
		else
			s = DOMString(QString::number(m_value));
	}
	else
	{
		// Handle non-numbers as specified by the string() function
		if(isnan(m_value))
			s = DOMString("NaN");
		else if(isinf(m_value) == 1)
			s = DOMString("Infinity");
		else if(isinf(m_value) == -1)
			s = DOMString("-Infinity");
	}

	return new StringImpl(s);
}

BooleanImpl *NumberImpl::toBoolean()
{
	if(isnan(m_value))
		return new BooleanImpl(false);
	else if(fabs(m_value) < DBL_EPSILON)
		return new BooleanImpl(false);
	
        return new BooleanImpl(true);
}

double NumberImpl::value() const
{
	return m_value;
}

// vim:ts=4:noet
