// -*- c-basic-offset: 2 -*-
/*
 *  Copyright (C) 1999-2001 Harri Porten (porten@kde.org)
 *  Copyright (C) 2001 Peter Kelly (pmk@post.com)
 *  Copyright (C) 2003, 2004, 2005, 2006, 2007, 2008 Apple Inc. All rights reserved.
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
 *  the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 *  Boston, MA 02110-1301, USA.
 *
 */

#ifndef JSString_h
#define JSString_h

#include "JSObject.h"
#include "ustring.h"

namespace KJS {

  class JSString : public JSCell {
  public:
    JSString(const UString& value) : m_value(value) { Heap::heap(this)->reportExtraMemoryCost(value.cost()); }
    enum HasOtherOwnerType { HasOtherOwner };
    JSString(const UString& value, HasOtherOwnerType) : m_value(value) { }

    const UString& value() const { return m_value; }

    bool getStringPropertySlot(ExecState*, const Identifier& propertyName, PropertySlot&);
    bool getStringPropertySlot(unsigned propertyName, PropertySlot&);

  private:
    virtual JSType type() const { return StringType; }

    virtual JSValue* toPrimitive(ExecState*, JSType preferred = UnspecifiedType) const;
    virtual bool getPrimitiveNumber(ExecState*, double& number, JSValue*& value);
    virtual bool toBoolean(ExecState*) const;
    virtual double toNumber(ExecState*) const;
    virtual JSObject* toObject(ExecState*) const;
    virtual UString toString(ExecState*) const;

    virtual JSObject* toThisObject(ExecState*) const;
    virtual UString toThisString(ExecState*) const;
    virtual JSString* toThisJSString(ExecState*);

    // Actually getPropertySlot, not getOwnPropertySlot (see JSCell).
    virtual bool getOwnPropertySlot(ExecState*, const Identifier& propertyName, PropertySlot&);
    virtual bool getOwnPropertySlot(ExecState*, unsigned propertyName, PropertySlot&);

    static JSValue* lengthGetter(ExecState*, const Identifier&, const PropertySlot&);
    static JSValue* indexGetter(ExecState*, const Identifier&, const PropertySlot&);
    static JSValue* indexNumericPropertyGetter(ExecState*, unsigned, const PropertySlot&);

    UString m_value;
  };

ALWAYS_INLINE bool JSString::getStringPropertySlot(ExecState* exec, const Identifier& propertyName, PropertySlot& slot)
{
    if (propertyName == exec->propertyNames().length) {
        slot.setCustom(this, lengthGetter);
        return true;
    }

    bool isStrictUInt32;
    unsigned i = propertyName.toStrictUInt32(&isStrictUInt32);
    if (isStrictUInt32 && i < static_cast<unsigned>(m_value.size())) {
        slot.setCustomIndex(this, i, indexGetter);
        return true;
    }

    return false;
}
    
ALWAYS_INLINE bool JSString::getStringPropertySlot(unsigned propertyName, PropertySlot& slot)
{
    if (propertyName < static_cast<unsigned>(m_value.size())) {
        slot.setCustomNumeric(this, indexNumericPropertyGetter);
        return true;
    }

    return false;
}

} // namespace

#endif //  JSString_h
