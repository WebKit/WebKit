/*
 *  Copyright (C) 1999-2000 Harri Porten (porten@kde.org)
 *  Copyright (C) 2007, 2008 Apple Inc. All rights reserved.
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

#ifndef StringObject_h
#define StringObject_h

#include "JSWrapperObject.h"
#include "JSString.h"

namespace KJS {

  class StringObject : public JSWrapperObject {
  public:
    StringObject(ExecState*, JSObject* prototype);
    StringObject(ExecState*, JSObject* prototype, const UString&);

    static StringObject* create(ExecState*, JSString*);

    virtual bool getOwnPropertySlot(ExecState*, const Identifier&, PropertySlot&);
    virtual bool getOwnPropertySlot(ExecState*, unsigned propertyName, PropertySlot&);

    virtual void put(ExecState* exec, const Identifier& propertyName, JSValue*);
    virtual bool deleteProperty(ExecState*, const Identifier& propertyName);
    virtual void getPropertyNames(ExecState*, PropertyNameArray&);

    virtual const ClassInfo* classInfo() const { return &info; }
    static const ClassInfo info;

    JSString* internalValue() const { return static_cast<JSString*>(JSWrapperObject::internalValue());}

  protected:
    StringObject(JSObject* prototype, JSString*);

  private:
    virtual UString toString(ExecState*) const;
    virtual UString toThisString(ExecState*) const;
    virtual JSString* toThisJSString(ExecState*);
  };

} // namespace KJS

#endif // StringObject_h
