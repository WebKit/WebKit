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

#ifndef JSObject_h
#define JSObject_h

#include "ArgList.h"
#include "ClassInfo.h"
#include "CommonIdentifiers.h"
#include "ExecState.h"
#include "JSNumberCell.h"
#include "JSType.h"
#include "PropertyMap.h"
#include "PropertySlot.h"
#include "ScopeChain.h"

namespace KJS {

    class InternalFunction;
    class PropertyNameArray;
    struct HashEntry;
    struct HashTable;

    // ECMA 262-3 8.6.1
    // Property attributes
    enum Attribute {
        None         = 0,
        ReadOnly     = 1 << 1,  // property can be only read, not written
        DontEnum     = 1 << 2,  // property doesn't appear in (for .. in ..)
        DontDelete   = 1 << 3,  // property can't be deleted
        Function     = 1 << 4,  // property is a function - only used by static hashtables
        IsGetterSetter = 1 << 5 // property is a getter or setter
    };

    class JSObject : public JSCell {
    public:
        /**
         * Creates a new JSObject with the specified prototype
         *
         * @param prototype The prototype
         */
        JSObject(JSValue* prototype);

        /**
         * Creates a new JSObject with a prototype of jsNull()
         * (that is, the ECMAScript "null" value, not a null object pointer).
         */
        JSObject();

        virtual void mark();
        virtual JSType type() const;

        bool inherits(const ClassInfo* classInfo) const { return isObject(classInfo); } // FIXME: Merge with isObject.

        JSValue* prototype() const;
        void setPrototype(JSValue* prototype);

        virtual UString className() const;

        JSValue* get(ExecState*, const Identifier& propertyName) const;
        JSValue* get(ExecState*, unsigned propertyName) const;

        bool getPropertySlot(ExecState*, const Identifier& propertyName, PropertySlot&);
        bool getPropertySlot(ExecState*, unsigned propertyName, PropertySlot&);

        virtual bool getOwnPropertySlot(ExecState*, const Identifier& propertyName, PropertySlot&);
        virtual bool getOwnPropertySlot(ExecState*, unsigned propertyName, PropertySlot&);

        virtual void put(ExecState*, const Identifier& propertyName, JSValue* value);
        virtual void put(ExecState*, unsigned propertyName, JSValue* value);

        virtual void putWithAttributes(ExecState*, const Identifier& propertyName, JSValue* value, unsigned attributes);
        virtual void putWithAttributes(ExecState*, unsigned propertyName, JSValue* value, unsigned attributes);

        bool propertyIsEnumerable(ExecState*, const Identifier& propertyName) const;

        bool hasProperty(ExecState*, const Identifier& propertyName) const;
        bool hasProperty(ExecState*, unsigned propertyName) const;
        bool hasOwnProperty(ExecState*, const Identifier& propertyName) const;

        virtual bool deleteProperty(ExecState*, const Identifier& propertyName);
        virtual bool deleteProperty(ExecState*, unsigned propertyName);

        virtual JSValue* defaultValue(ExecState*, JSType hint) const;

        virtual bool implementsHasInstance() const;
        virtual bool hasInstance(ExecState*, JSValue*);

        virtual void getPropertyNames(ExecState*, PropertyNameArray&);

        virtual JSValue* toPrimitive(ExecState*, JSType preferredType = UnspecifiedType) const;
        virtual bool getPrimitiveNumber(ExecState*, double& number, JSValue*& value);
        virtual bool toBoolean(ExecState*) const;
        virtual double toNumber(ExecState*) const;
        virtual UString toString(ExecState*) const;
        virtual JSObject* toObject(ExecState*) const;

        virtual JSObject* toThisObject(ExecState*) const;
        virtual JSGlobalObject* toGlobalObject(ExecState*) const;

        virtual bool getPropertyAttributes(ExecState*, const Identifier& propertyName, unsigned& attributes) const;

        // WebCore uses this to make document.all and style.filter undetectable
        virtual bool masqueradeAsUndefined() const { return false; }

        // This get function only looks at the property map.
        JSValue* getDirect(const Identifier& propertyName) const { return m_propertyMap.get(propertyName); }
        JSValue** getDirectLocation(const Identifier& propertyName) { return m_propertyMap.getLocation(propertyName); }
        JSValue** getDirectLocation(const Identifier& propertyName, bool& isWriteable) { return m_propertyMap.getLocation(propertyName, isWriteable); }
        void putDirect(const Identifier& propertyName, JSValue* value, unsigned attr = 0);
        void putDirect(ExecState*, const Identifier& propertyName, int value, unsigned attr = 0);
        void removeDirect(const Identifier& propertyName);
        bool hasCustomProperties() { return !m_propertyMap.isEmpty(); }

        // convenience to add a function property under the function's own built-in name
        void putDirectFunction(ExecState*, InternalFunction*, unsigned attr = 0);

        void fillGetterPropertySlot(PropertySlot&, JSValue** location);

        virtual void defineGetter(ExecState*, const Identifier& propertyName, JSObject* getterFunction);
        virtual void defineSetter(ExecState*, const Identifier& propertyName, JSObject* setterFunction);
        virtual JSValue* lookupGetter(ExecState*, const Identifier& propertyName);
        virtual JSValue* lookupSetter(ExecState*, const Identifier& propertyName);

        virtual bool isActivationObject() const { return false; }
        virtual bool isGlobalObject() const { return false; }
        virtual bool isVariableObject() const { return false; }

        virtual bool isWatchdogException() const { return false; }
        
        virtual bool isNotAnObjectErrorStub() const { return false; }

    protected:
        PropertyMap m_propertyMap;
        bool getOwnPropertySlotForWrite(ExecState*, const Identifier&, PropertySlot&, bool& slotIsWriteable);

    private:
        const HashEntry* findPropertyHashEntry(ExecState*, const Identifier& propertyName) const;
        JSValue* m_prototype;
    };

  JSObject* constructEmptyObject(ExecState*);

inline JSObject::JSObject(JSValue* prototype)
    : m_prototype(prototype)
{
    ASSERT(prototype);
    ASSERT(prototype == jsNull() || Heap::heap(this) == Heap::heap(prototype));
}

inline JSObject::JSObject()
    : m_prototype(jsNull())
{
}

inline JSValue* JSObject::prototype() const
{
    return m_prototype;
}

inline void JSObject::setPrototype(JSValue* prototype)
{
    ASSERT(prototype);
    m_prototype = prototype;
}

inline bool JSCell::isObject(const ClassInfo* info) const
{
    for (const ClassInfo* ci = classInfo(); ci; ci = ci->parentClass) {
        if (ci == info)
            return true;
    }
    return false;
}

// this method is here to be after the inline declaration of JSCell::isObject
inline bool JSValue::isObject(const ClassInfo* classInfo) const
{
    return !JSImmediate::isImmediate(this) && asCell()->isObject(classInfo);
}

inline JSValue* JSObject::get(ExecState* exec, const Identifier& propertyName) const
{
    PropertySlot slot(const_cast<JSObject*>(this));
    if (const_cast<JSObject*>(this)->getPropertySlot(exec, propertyName, slot))
        return slot.getValue(exec, propertyName);
    
    return jsUndefined();
}

inline JSValue* JSObject::get(ExecState* exec, unsigned propertyName) const
{
    PropertySlot slot(const_cast<JSObject*>(this));
    if (const_cast<JSObject*>(this)->getPropertySlot(exec, propertyName, slot))
        return slot.getValue(exec, propertyName);

    return jsUndefined();
}

// It may seem crazy to inline a function this large but it makes a big difference
// since this is function very hot in variable lookup
inline bool JSObject::getPropertySlot(ExecState* exec, const Identifier& propertyName, PropertySlot& slot)
{
    JSObject* object = this;
    while (true) {
        if (object->getOwnPropertySlot(exec, propertyName, slot))
            return true;

        JSValue* prototype = object->m_prototype;
        if (!prototype->isObject())
            return false;

        object = static_cast<JSObject*>(prototype);
    }
}

inline bool JSObject::getPropertySlot(ExecState* exec, unsigned propertyName, PropertySlot& slot)
{
    JSObject* object = this;

    while (true) {
        if (object->getOwnPropertySlot(exec, propertyName, slot))
            return true;

        JSValue* prototype = object->m_prototype;
        if (!prototype->isObject())
            break;

        object = static_cast<JSObject*>(prototype);
    }

    return false;
}

// It may seem crazy to inline a function this large, especially a virtual function,
// but it makes a big difference to property lookup that derived classes can inline their
// base class call to this.
ALWAYS_INLINE bool JSObject::getOwnPropertySlotForWrite(ExecState* exec, const Identifier& propertyName, PropertySlot& slot, bool& slotIsWriteable)
{
    if (JSValue** location = getDirectLocation(propertyName, slotIsWriteable)) {
        if (m_propertyMap.hasGetterSetterProperties() && location[0]->type() == GetterSetterType) {
            slotIsWriteable = false;
            fillGetterPropertySlot(slot, location);
        } else
            slot.setValueSlot(location);
        return true;
    }

    // non-standard Netscape extension
    if (propertyName == exec->propertyNames().underscoreProto) {
        slot.setValueSlot(&m_prototype);
        slotIsWriteable = false;
        return true;
    }

    return false;
}

// It may seem crazy to inline a function this large, especially a virtual function,
// but it makes a big difference to property lookup that derived classes can inline their
// base class call to this.
ALWAYS_INLINE bool JSObject::getOwnPropertySlot(ExecState* exec, const Identifier& propertyName, PropertySlot& slot)
{
    if (JSValue** location = getDirectLocation(propertyName)) {
        if (m_propertyMap.hasGetterSetterProperties() && location[0]->type() == GetterSetterType)
            fillGetterPropertySlot(slot, location);
        else
            slot.setValueSlot(location);
        return true;
    }

    // non-standard Netscape extension
    if (propertyName == exec->propertyNames().underscoreProto) {
        slot.setValueSlot(&m_prototype);
        return true;
    }

    return false;
}

inline void JSObject::putDirect(const Identifier& propertyName, JSValue* value, unsigned attr)
{
    ASSERT(!Heap::heap(value) || Heap::heap(value) == Heap::heap(this));

    m_propertyMap.put(propertyName, value, attr);
}

inline void JSObject::putDirect(ExecState* exec, const Identifier& propertyName, int value, unsigned attr)
{
    m_propertyMap.put(propertyName, jsNumber(exec, value), attr);
}

inline JSValue* JSObject::toPrimitive(ExecState* exec, JSType preferredType) const
{
    return defaultValue(exec, preferredType);
}

inline JSValue* JSValue::get(ExecState* exec, const Identifier& propertyName) const
{
    if (UNLIKELY(JSImmediate::isImmediate(this))) {
        JSObject* prototype = JSImmediate::prototype(this, exec);
        PropertySlot slot(const_cast<JSValue*>(this));
        if (!prototype->getPropertySlot(exec, propertyName, slot))
            return jsUndefined();
        return slot.getValue(exec, propertyName);
    }
    JSCell* cell = static_cast<JSCell*>(const_cast<JSValue*>(this));
    PropertySlot slot(cell);
    while (true) {
        if (cell->getOwnPropertySlot(exec, propertyName, slot))
            return slot.getValue(exec, propertyName);
        ASSERT(cell->isObject());
        JSValue* prototype = static_cast<JSObject*>(cell)->prototype();
        if (!prototype->isObject())
            return jsUndefined();
        cell = static_cast<JSCell*>(prototype);
    }
}

inline JSValue* JSValue::get(ExecState* exec, unsigned propertyName) const
{
    if (UNLIKELY(JSImmediate::isImmediate(this))) {
        JSObject* prototype = JSImmediate::prototype(this, exec);
        PropertySlot slot(const_cast<JSValue*>(this));
        if (!prototype->getPropertySlot(exec, propertyName, slot))
            return jsUndefined();
        return slot.getValue(exec, propertyName);
    }
    JSCell* cell = const_cast<JSCell*>(asCell());
    PropertySlot slot(cell);
    while (true) {
        if (cell->getOwnPropertySlot(exec, propertyName, slot))
            return slot.getValue(exec, propertyName);
        ASSERT(cell->isObject());
        JSValue* prototype = static_cast<JSObject*>(cell)->prototype();
        if (!prototype->isObject())
            return jsUndefined();
        cell = static_cast<JSCell*>(prototype);
    }
}

inline void JSValue::put(ExecState* exec, const Identifier& propertyName, JSValue* value)
{
    if (UNLIKELY(JSImmediate::isImmediate(this))) {
        JSImmediate::toObject(this, exec)->put(exec, propertyName, value);
        return;
    }
    asCell()->put(exec, propertyName, value);
}

inline void JSValue::put(ExecState* exec, unsigned propertyName, JSValue* value)
{
    if (UNLIKELY(JSImmediate::isImmediate(this))) {
        JSImmediate::toObject(this, exec)->put(exec, propertyName, value);
        return;
    }
    asCell()->put(exec, propertyName, value);
}

} // namespace KJS

#endif // JSObject_h
