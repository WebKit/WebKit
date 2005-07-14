// -*- c-basic-offset: 2 -*-
/*
 *  This file is part of the KDE libraries
 *  Copyright (C) 2003 Apple Computer, Inc
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
 *  the Free Software Foundation, Inc., 51 Franklin Steet, Fifth Floor,
 *  Boston, MA 02110-1301, USA.
 *
 */

#ifndef _KJS_REFERENCE_H_
#define _KJS_REFERENCE_H_

#include "identifier.h"
#include "value.h"

namespace KJS {

  class Reference {
    friend class ReferenceList;
    friend class ReferenceListIterator;
    friend class ProtectedReference;
  public:
    Reference(const Object& b, const Identifier& p);
    Reference(const Object& b, unsigned p);
    Reference(ObjectImp *b, const Identifier& p);
    Reference(ObjectImp *b, unsigned p);
    Reference(const Null& b, const Identifier& p);
    Reference(const Null& b, unsigned p);
    static Reference makeValueReference(const Value& v);
    
    /**
     * Performs the GetBase type conversion operation on this value (ECMA 8.7)
     *
     * Since references are supposed to have an Object or null as their base,
     * this method is guaranteed to return either Null() or an Object value.
     */
    Value getBase(ExecState *exec) const;

    /**
     * Performs the GetPropertyName type conversion operation on this value
     * (ECMA 8.7)
     */
    Identifier getPropertyName(ExecState *exec) const;

    /**
     * Performs the GetValue type conversion operation on this value
     * (ECMA 8.7.1)
     */
    Value getValue(ExecState *exec) const;

    /**
     * Performs the PutValue type conversion operation on this value
     * (ECMA 8.7.1)
     */
    void putValue(ExecState *exec, const Value &w);
    bool deleteValue(ExecState *exec);

    ValueImp *baseIfMutable() const { return baseIsValue ? 0 : base.imp(); }

  private:
    Reference() { }

    Value base;
    unsigned propertyNameAsNumber;
    bool baseIsValue;
    bool propertyNameIsNumber;
    mutable Identifier prop;
  };
}

#endif
