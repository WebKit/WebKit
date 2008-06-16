// -*- c-basic-offset: 2 -*-
/*
 *  Copyright (C) 1999-2000 Harri Porten (porten@kde.org)
 *  Copyright (C) 2003, 2007, 2008 Apple Inc. All rights reserved.
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

#ifndef JSArray_h
#define JSArray_h

#include "JSObject.h"

namespace KJS {

  struct ArrayStorage;

  class JSArray : public JSObject {
  public:
    JSArray(JSObject* prototype, unsigned initialLength);
    JSArray(JSObject* prototype, const ArgList& initialValues);
    ~JSArray();

    virtual bool getOwnPropertySlot(ExecState*, const Identifier& propertyName, PropertySlot&);
    virtual bool getOwnPropertySlot(ExecState*, unsigned propertyName, PropertySlot&);
    virtual void put(ExecState*, const Identifier& propertyName, JSValue*);
    virtual void put(ExecState*, unsigned propertyName, JSValue*);
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

  protected:
    void* lazyCreationData();
    void setLazyCreationData(void*);

  private:
    static JSValue* lengthGetter(ExecState*, const Identifier&, const PropertySlot&);
    bool inlineGetOwnPropertySlot(ExecState*, unsigned propertyName, PropertySlot&);

    void setLength(unsigned);
    bool increaseVectorLength(unsigned newLength);
    
    unsigned compactForSorting();

    enum ConsistencyCheckType { NormalConsistencyCheck, DestructorConsistencyCheck, SortConsistencyCheck };
    void checkConsistency(ConsistencyCheckType = NormalConsistencyCheck);

    unsigned m_length;
    unsigned m_vectorLength;
    ArrayStorage* m_storage;
  };

} // namespace KJS

#endif
