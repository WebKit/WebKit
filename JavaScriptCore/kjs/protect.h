// -*- c-basic-offset: 2 -*-
/*
 *  This file is part of the KDE libraries
 *  Copyright (C) 2004 Apple Computer, Inc.
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Library General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Library General Public License for more details.
 *
 *  You should have received a copy of the GNU Library General Public License
 *  along with this library; see the file COPYING.LIB.  If not, write to
 *  the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 *  Boston, MA 02111-1307, USA.
 *
 */


#ifndef _KJS_PROTECT_H_
#define _KJS_PROTECT_H_

#include "reference.h"
#include "value.h"
#include "protected_values.h"

namespace KJS {

    inline void gcProtect(ValueImp *val) 
      { 
	ProtectedValues::increaseProtectCount(val);
      }
    inline void gcUnprotect(ValueImp *val)
      { 
	ProtectedValues::decreaseProtectCount(val);
      }

    inline void gcProtectNullTolerant(ValueImp *val) 
      {
	if (val) gcProtect(val);
      }

    inline void gcUnprotectNullTolerant(ValueImp *val) 
      {
	if (val) gcUnprotect(val);
      }
    
class ProtectedValue {
public:
    ProtectedValue() : m_value(0) { }
    ProtectedValue(ValueImp *v) : m_value(v) { gcProtectNullTolerant(v); }
    ProtectedValue(const ProtectedValue& v) : m_value(v.m_value) { gcProtectNullTolerant(m_value); }
    ~ProtectedValue() { gcUnprotectNullTolerant(m_value); }
    ProtectedValue& operator=(ValueImp *v)
    {
        gcProtectNullTolerant(v);
        gcUnprotectNullTolerant(m_value);
        m_value = v;
        return *this;
    }
    ProtectedValue& operator=(const ProtectedValue& v) { return *this = v.m_value; }
    operator ValueImp *() const { return m_value; }
    ValueImp *operator->() const { return m_value; }
protected:
    ValueImp *m_value;
};

} // namespace

#endif
