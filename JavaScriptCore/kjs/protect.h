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

#include "object.h"
#include "reference.h"
#include "value.h"

namespace KJS {

    inline void gcProtect(ValueImp *) {}
    inline void gcUnprotect(ValueImp *) {}
    
    class ProtectedValue : public Value {
    public:
      ProtectedValue() : Value() {}
      ProtectedValue(const Value&v)  : Value(v) { gcProtect(v.imp()); };
      ~ProtectedValue() { gcUnprotect(imp());}
      ProtectedValue& operator=(const Value &v)
	{ 
	  ValueImp *old = imp();
	  Value::operator=(v); 
	  gcProtect(v.imp());
	  gcUnprotect(old); 
	  return *this;
	}
    private:
      explicit ProtectedValue(ValueImp *v);
    };


    class ProtectedObject : public Object {
    public:
      ProtectedObject() : Object() {}
      ProtectedObject(const Object&o)  : Object(o) { gcProtect(o.imp()); };
      ~ProtectedObject() { gcUnprotect(imp());}
      ProtectedObject& operator=(const Object &o)
	{ 
	  ValueImp *old = imp();
	  Object::operator=(o); 
	  gcProtect(o.imp());
	  gcUnprotect(old); 
	  return *this;
	}
    private:
      explicit ProtectedObject(ObjectImp *o);
    };


    class ProtectedReference : public Reference {
    public:
      ProtectedReference(const Reference&r)  : Reference(r) { gcProtect(r.base.imp()); };
      ~ProtectedReference() { gcUnprotect(base.imp());}
      ProtectedReference& operator=(const Reference &r)
	{ 
	  ValueImp *old = base.imp();
	  Reference::operator=(r); 
	  gcProtect(r.base.imp());
	  gcUnprotect(old); 
	  return *this;
	}
    private:
      ProtectedReference();
      ProtectedReference(const Object& b, const Identifier& p);
      ProtectedReference(const Object& b, unsigned p);
      ProtectedReference(ObjectImp *b, const Identifier& p);
      ProtectedReference(ObjectImp *b, unsigned p);
      ProtectedReference(const Null& b, const Identifier& p);
      ProtectedReference(const Null& b, unsigned p);
    };

}

#endif
