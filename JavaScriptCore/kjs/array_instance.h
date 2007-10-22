// -*- c-basic-offset: 2 -*-
/*
 *  Copyright (C) 1999-2000 Harri Porten (porten@kde.org)
 *  Copyright (C) 2003, 2007 Apple Inc. All rights reserved.
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

#ifndef ARRAY_INSTANCE_H
#define ARRAY_INSTANCE_H

#include "object.h"

namespace KJS {

  struct ArrayStorage;

  class ArrayInstance : public JSObject {
  public:
    ArrayInstance(JSObject* prototype, unsigned initialLength);
    ArrayInstance(JSObject* prototype, const List& initialValues);
    ~ArrayInstance();

    virtual bool getOwnPropertySlot(ExecState*, const Identifier& propertyName, PropertySlot&);
    virtual bool getOwnPropertySlot(ExecState*, unsigned propertyName, PropertySlot&);
    virtual void put(ExecState*, const Identifier& propertyName, JSValue*, int attributes = None);
    virtual void put(ExecState*, unsigned propertyName, JSValue*, int attributes = None);
    virtual bool deleteProperty(ExecState *, const Identifier& propertyName);
    virtual bool deleteProperty(ExecState *, unsigned propertyName);
    virtual void getPropertyNames(ExecState*, PropertyNameArray&);

    virtual void mark();

    virtual const ClassInfo* classInfo() const { return &info; }
    static const ClassInfo info;

    unsigned getLength() const { return m_length; }
    JSValue* getItem(unsigned) const;

    void sort(ExecState*);
    void sort(ExecState*, JSObject* compareFunction);

  private:
    static JSValue* lengthGetter(ExecState*, JSObject*, const Identifier&, const PropertySlot&);
    bool inlineGetOwnPropertySlot(ExecState*, unsigned propertyName, PropertySlot&);

    void setLength(unsigned);
    void increaseVectorLength(unsigned newLength);
    
    unsigned compactForSorting();    

    unsigned m_length;
    unsigned m_vectorLength;
    ArrayStorage* m_storage;
  };

} // namespace KJS

#endif
