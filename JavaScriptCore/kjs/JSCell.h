/*
 *  Copyright (C) 1999-2001 Harri Porten (porten@kde.org)
 *  Copyright (C) 2001 Peter Kelly (pmk@post.com)
 *  Copyright (C) 2003, 2004, 2005, 2007, 2008 Apple Inc. All rights reserved.
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

#ifndef JSCell_h
#define JSCell_h

#include "StructureID.h"
#include "JSValue.h"
#include "collector.h"

namespace JSC {

    class JSCell : public JSValue {
        friend class Heap;
        friend class GetterSetter;
        friend class JSObject;
        friend class JSPropertyNameIterator;
        friend class JSValue;
        friend class JSNumberCell;
        friend class JSString;
        friend class Machine;
        friend class CTI;
    private:
        JSCell(StructureID*);
        virtual ~JSCell();

    public:
        // Querying the type.
        bool isNumber() const;
        virtual bool isString() const;
        virtual bool isGetterSetter() const;
        virtual bool isObject() const;
        virtual bool isObject(const ClassInfo*) const;

        StructureID* structureID() const;

        // Extracting the value.
        bool getNumber(double&) const;
        double getNumber() const; // NaN if not a number
        bool getString(UString&) const;
        UString getString() const; // null string if not a string
        JSObject* getObject(); // NULL if not an object
        const JSObject* getObject() const; // NULL if not an object
        
        virtual CallType getCallData(CallData&);
        virtual ConstructType getConstructData(ConstructData&);

        // Extracting integer values.
        virtual bool getUInt32(uint32_t&) const;
        virtual bool getTruncatedInt32(int32_t&) const;
        virtual bool getTruncatedUInt32(uint32_t&) const;

        // Basic conversions.
        virtual JSValue* toPrimitive(ExecState*, PreferredPrimitiveType) const = 0;
        virtual bool getPrimitiveNumber(ExecState*, double& number, JSValue*&) = 0;
        virtual bool toBoolean(ExecState*) const = 0;
        virtual double toNumber(ExecState*) const = 0;
        virtual UString toString(ExecState*) const = 0;
        virtual JSObject* toObject(ExecState*) const = 0;

        // WebCore uses this to make document.all and style.filter undetectable
        virtual bool masqueradeAsUndefined() const { return false; }

        // Garbage collection.
        void* operator new(size_t, ExecState*);
        void* operator new(size_t, void* placementNewDestination) { return placementNewDestination; }
        virtual void mark();
        bool marked() const;

        // Object operations, with the toObject operation included.
        virtual const ClassInfo* classInfo() const;
        virtual void put(ExecState*, const Identifier& propertyName, JSValue*, PutPropertySlot&);
        virtual void put(ExecState*, unsigned propertyName, JSValue*);
        virtual bool deleteProperty(ExecState*, const Identifier& propertyName);
        virtual bool deleteProperty(ExecState*, unsigned propertyName);

        virtual JSObject* toThisObject(ExecState*) const;
        virtual UString toThisString(ExecState*) const;
        virtual JSString* toThisJSString(ExecState*);
        virtual JSValue* getJSNumber();
        void* vptr() { return *reinterpret_cast<void**>(this); }

    private:
        // Base implementation, but for non-object classes implements getPropertySlot.
        virtual bool getOwnPropertySlot(ExecState*, const Identifier& propertyName, PropertySlot&);
        virtual bool getOwnPropertySlot(ExecState*, unsigned propertyName, PropertySlot&);
        
        StructureID* m_structureID;
    };

    inline JSCell::JSCell(StructureID* structureID)
        : m_structureID(structureID)
    {
    }

    inline JSCell::~JSCell()
    {
    }

    inline bool JSCell::isNumber() const
    {
        return Heap::isNumber(const_cast<JSCell*>(this));
    }

    inline StructureID* JSCell::structureID() const
    {
        return m_structureID;
    }

    inline bool JSCell::marked() const
    {
        return Heap::isCellMarked(this);
    }

    inline void JSCell::mark()
    {
        return Heap::markCell(this);
    }

    ALWAYS_INLINE JSCell* JSValue::asCell()
    {
        ASSERT(!JSImmediate::isImmediate(this));
        return static_cast<JSCell*>(this);
    }

    ALWAYS_INLINE const JSCell* JSValue::asCell() const
    {
        ASSERT(!JSImmediate::isImmediate(this));
        return static_cast<const JSCell*>(this);
    }


    // --- JSValue inlines ----------------------------

    inline bool JSValue::isNumber() const
    {
        return JSImmediate::isNumber(this) || (!JSImmediate::isImmediate(this) && asCell()->isNumber());
    }

    inline bool JSValue::isString() const
    {
        return !JSImmediate::isImmediate(this) && asCell()->isString();
    }

    inline bool JSValue::isGetterSetter() const
    {
        return !JSImmediate::isImmediate(this) && asCell()->isGetterSetter();
    }

    inline bool JSValue::isObject() const
    {
        return !JSImmediate::isImmediate(this) && asCell()->isObject();
    }

    inline bool JSValue::getNumber(double& v) const
    {
        if (JSImmediate::isImmediate(this)) {
            v = JSImmediate::toDouble(this);
            return true;
        }
        return asCell()->getNumber(v);
    }

    inline double JSValue::getNumber() const
    {
        return JSImmediate::isImmediate(this) ? JSImmediate::toDouble(this) : asCell()->getNumber();
    }

    inline bool JSValue::getString(UString& s) const
    {
        return !JSImmediate::isImmediate(this) && asCell()->getString(s);
    }

    inline UString JSValue::getString() const
    {
        return JSImmediate::isImmediate(this) ? UString() : asCell()->getString();
    }

    inline JSObject* JSValue::getObject()
    {
        return JSImmediate::isImmediate(this) ? 0 : asCell()->getObject();
    }

    inline const JSObject* JSValue::getObject() const
    {
        return JSImmediate::isImmediate(this) ? 0 : asCell()->getObject();
    }

    inline CallType JSValue::getCallData(CallData& callData)
    {
        return JSImmediate::isImmediate(this) ? CallTypeNone : asCell()->getCallData(callData);
    }

    inline ConstructType JSValue::getConstructData(ConstructData& constructData)
    {
        return JSImmediate::isImmediate(this) ? ConstructTypeNone : asCell()->getConstructData(constructData);
    }

    ALWAYS_INLINE bool JSValue::getUInt32(uint32_t& v) const
    {
        return JSImmediate::isImmediate(this) ? JSImmediate::getUInt32(this, v) : asCell()->getUInt32(v);
    }

    ALWAYS_INLINE bool JSValue::getTruncatedInt32(int32_t& v) const
    {
        return JSImmediate::isImmediate(this) ? JSImmediate::getTruncatedInt32(this, v) : asCell()->getTruncatedInt32(v);
    }

    inline bool JSValue::getTruncatedUInt32(uint32_t& v) const
    {
        return JSImmediate::isImmediate(this) ? JSImmediate::getTruncatedUInt32(this, v) : asCell()->getTruncatedUInt32(v);
    }

    inline void JSValue::mark()
    {
        ASSERT(!JSImmediate::isImmediate(this)); // callers should check !marked() before calling mark()
        asCell()->mark();
    }

    inline bool JSValue::marked() const
    {
        return JSImmediate::isImmediate(this) || asCell()->marked();
    }

    inline JSValue* JSValue::toPrimitive(ExecState* exec, PreferredPrimitiveType preferredType) const
    {
        return JSImmediate::isImmediate(this) ? const_cast<JSValue*>(this) : asCell()->toPrimitive(exec, preferredType);
    }

    inline bool JSValue::getPrimitiveNumber(ExecState* exec, double& number, JSValue*& value)
    {
        if (JSImmediate::isImmediate(this)) {
            number = JSImmediate::toDouble(this);
            value = this;
            return true;
        }
        return asCell()->getPrimitiveNumber(exec, number, value);
    }

    inline bool JSValue::toBoolean(ExecState* exec) const
    {
        return JSImmediate::isImmediate(this) ? JSImmediate::toBoolean(this) : asCell()->toBoolean(exec);
    }

    ALWAYS_INLINE double JSValue::toNumber(ExecState* exec) const
    {
        return JSImmediate::isImmediate(this) ? JSImmediate::toDouble(this) : asCell()->toNumber(exec);
    }

    inline UString JSValue::toString(ExecState* exec) const
    {
        return JSImmediate::isImmediate(this) ? JSImmediate::toString(this) : asCell()->toString(exec);
    }

    inline JSObject* JSValue::toObject(ExecState* exec) const
    {
        return JSImmediate::isImmediate(this) ? JSImmediate::toObject(this, exec) : asCell()->toObject(exec);
    }

    inline JSObject* JSValue::toThisObject(ExecState* exec) const
    {
        if (UNLIKELY(JSImmediate::isImmediate(this)))
            return JSImmediate::toObject(this, exec);
        return asCell()->toThisObject(exec);
    }

    inline UString JSValue::toThisString(ExecState* exec) const
    {
        return JSImmediate::isImmediate(this) ? JSImmediate::toString(this) : asCell()->toThisString(exec);
    }

    inline JSValue* JSValue::getJSNumber()
    {

        return JSImmediate::isNumber(this) ? this : (JSImmediate::isImmediate(this) ? 0 : asCell()->getJSNumber());
    }

} // namespace JSC

#endif // JSCell_h
