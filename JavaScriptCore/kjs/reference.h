// -*- c-basic-offset: 2 -*-
/*
 *  This file is part of the KDE libraries
 *  Copyright (C) 2002 Apple Computer, Inc
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

#ifndef _KJS_REFERENCE_H_
#define _KJS_REFERENCE_H_

#include "value.h"

namespace KJS {

  class Reference : private Value {
    friend class ReferenceList;
    friend class ReferenceListIterator;
  public:
    Reference(const Object& b, const UString& p);
    Reference(const Object& b, unsigned p);
    Reference(const Null& b, const UString& p);
    Reference(const Null& b, unsigned p);
    Reference(ReferenceImp *v);
    
    /**
     * Converts a Value into an Reference. If the value's type is not
     * ReferenceType, a null object will be returned (i.e. one with it's
     * internal pointer set to 0). If you do not know for sure whether the
     * value is of type ReferenceType, you should check the @ref isNull()
     * methods afterwards before calling any methods on the returned value.
     *
     * @return The value converted to an Reference
     */
    static Reference dynamicCast(const Value &v);

    /**
     * Performs the GetBase type conversion operation on this value (ECMA 8.7)
     *
     * Since references are supposed to have an Object or null as their base,
     * this method is guaranteed to return either Null() or an Object value.
     */
    Value getBase(ExecState *exec) const { return rep->dispatchGetBase(exec); }

    /**
     * Performs the GetPropertyName type conversion operation on this value
     * (ECMA 8.7)
     */
    UString getPropertyName(ExecState *exec) const { return rep->dispatchGetPropertyName(exec); }

    /**
     * Performs the GetValue type conversion operation on this value
     * (ECMA 8.7.1)
     */
    Value getValue(ExecState *exec) const { return rep->dispatchGetValue(exec); }

    /**
     * Performs the PutValue type conversion operation on this value
     * (ECMA 8.7.1)
     */
    void putValue(ExecState *exec, const Value &w) { rep->dispatchPutValue(exec, w); }
    bool deleteValue(ExecState *exec) { return rep->dispatchDeleteValue(exec); }
    bool isMutable() { return type() == ReferenceType; }
  };

  class ConstReference : public Reference {
  public:
    ConstReference(ValueImp *v);
  };
}

#endif
