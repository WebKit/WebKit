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
    virtual ~JSArray();

    virtual bool getOwnPropertySlot(ExecState*, const Identifier& propertyName, PropertySlot&);
    virtual bool getOwnPropertySlot(ExecState*, unsigned propertyName, PropertySlot&);
    virtual void put(ExecState*, unsigned propertyName, JSValue*); // FIXME: Make protected and add setItem.

    static const ClassInfo info;

    unsigned getLength() const { return m_length; }
    void setLength(unsigned); // OK to use on new arrays, but not if it might be a RegExpMatchArray.

    void sort(ExecState*);
    void sort(ExecState*, JSValue* compareFunction, CallType, const CallData&);

  protected:
    virtual void put(ExecState*, const Identifier& propertyName, JSValue*);
    virtual bool deleteProperty(ExecState*, const Identifier& propertyName);
    virtual bool deleteProperty(ExecState*, unsigned propertyName);
    virtual void getPropertyNames(ExecState*, PropertyNameArray&);
    virtual void mark();

    void* lazyCreationData();
    void setLazyCreationData(void*);

  private:
    virtual const ClassInfo* classInfo() const { return &info; }

    static JSValue* lengthGetter(ExecState*, const Identifier&, const PropertySlot&);

    bool getOwnPropertySlotSlowCase(ExecState*, unsigned propertyName, PropertySlot&);
    void putSlowCase(ExecState*, unsigned propertyName, JSValue*);

    bool increaseVectorLength(unsigned newLength);
    
    unsigned compactForSorting();

    enum ConsistencyCheckType { NormalConsistencyCheck, DestructorConsistencyCheck, SortConsistencyCheck };
    void checkConsistency(ConsistencyCheckType = NormalConsistencyCheck);

    unsigned m_length;
    unsigned m_fastAccessCutoff;
    ArrayStorage* m_storage;
  };

  JSArray* constructEmptyArray(ExecState*);
  JSArray* constructEmptyArray(ExecState*, unsigned initialLength);
  JSArray* constructArray(ExecState*, JSValue* singleItemValue);
  JSArray* constructArray(ExecState*, const ArgList& values);

} // namespace KJS

#endif
