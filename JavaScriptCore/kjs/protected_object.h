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

#ifndef KJS_PROTECTED_OBJECT_H
#define KJS_PROTECTED_OBJECT_H

#include "protect.h"
#include "object.h"
#include "reference.h"

namespace KJS {

class ProtectedObject : private ProtectedValue {
public:
    ProtectedObject() { }
    ProtectedObject(ObjectImp *v) : ProtectedValue(v) { }
    ProtectedObject(const ProtectedObject& v) : ProtectedValue(v) { }
    ProtectedObject& operator=(ObjectImp *v) { ProtectedValue::operator=(v); return *this; }
    ProtectedObject& operator=(const ProtectedObject& v) { ProtectedValue::operator=(v); return *this; }
    operator ValueImp *() const { return m_value; }
    operator ObjectImp *() const { return static_cast<ObjectImp *>(m_value); }
    ObjectImp *operator->() const { return static_cast<ObjectImp *>(m_value); }
};

    class ProtectedReference : public Reference {
    public:
      ProtectedReference(const Reference& r) : Reference(r) { gcProtectNullTolerant(r.base); };
      ~ProtectedReference() { gcUnprotectNullTolerant(base);}
      ProtectedReference& operator=(const Reference &r)
	{ 
	  ValueImp *old = base;
	  Reference::operator=(r); 
	  gcProtectNullTolerant(r.base);
	  gcUnprotectNullTolerant(old); 
	  return *this;
	}
    private:
      ProtectedReference();
      ProtectedReference(ObjectImp *b, const Identifier& p);
      ProtectedReference(ObjectImp *b, unsigned p);
      ProtectedReference(const Identifier& p);
      ProtectedReference(unsigned p);
    };

} // namespace

#endif
