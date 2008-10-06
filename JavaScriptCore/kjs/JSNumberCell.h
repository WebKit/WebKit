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

#ifndef JSNumberCell_h
#define JSNumberCell_h

#include "ExecState.h"
#include "JSCell.h"
#include "JSImmediate.h"
#include "collector.h"
#include "ustring.h"
#include <stddef.h> // for size_t

namespace JSC {

    class Identifier;
    class JSCell;
    class JSObject;
    class JSString;
    class PropertySlot;

    struct ClassInfo;
    struct Instruction;

    class JSNumberCell : public JSCell {
        friend class CTI;
        friend JSValue* jsNumberCell(JSGlobalData*, double);
        friend JSValue* jsNaN(JSGlobalData*);
        friend JSValue* jsNumberCell(ExecState*, double);
        friend JSValue* jsNaN(ExecState*);
    public:
        double value() const { return m_value; }

        virtual JSValue* toPrimitive(ExecState*, PreferredPrimitiveType) const;
        virtual bool getPrimitiveNumber(ExecState*, double& number, JSValue*& value);
        bool toBoolean() const { return m_value < 0.0 || m_value > 0.0; /* false for NaN */ }
        virtual double toNumber(ExecState*) const;
        virtual UString toString(ExecState*) const;
        virtual JSObject* toObject(ExecState*) const;

        virtual UString toThisString(ExecState*) const;
        virtual JSObject* toThisObject(ExecState*) const;
        virtual JSValue* getJSNumber();

        int32_t toInt32() const;
        uint32_t toUInt32() const;

        void* operator new(size_t size, ExecState* exec)
        {
    #ifdef JAVASCRIPTCORE_BUILDING_ALL_IN_ONE_FILE
            return exec->heap()->inlineAllocateNumber(size);
    #else
            return exec->heap()->allocateNumber(size);
    #endif
        }

        void* operator new(size_t size, JSGlobalData* globalData)
        {
    #ifdef JAVASCRIPTCORE_BUILDING_ALL_IN_ONE_FILE
            return globalData->heap.inlineAllocateNumber(size);
    #else
            return globalData->heap.allocateNumber(size);
    #endif
        }

        static PassRefPtr<StructureID> createStructureID(JSValue* proto) { return StructureID::create(proto, TypeInfo(NumberType, NeedsThisConversion)); }

    private:
        JSNumberCell(JSGlobalData* globalData, double value)
            : JSCell(globalData->numberStructureID.get())
            , m_value(value)
        {
        }

        JSNumberCell(ExecState* exec, double value)
            : JSCell(exec->globalData().numberStructureID.get())
            , m_value(value)
        {
        }

        virtual bool getUInt32(uint32_t&) const;
        virtual bool getTruncatedInt32(int32_t&) const;
        virtual bool getTruncatedUInt32(uint32_t&) const;

        double m_value;
    };

    extern const double NaN;
    extern const double Inf;

    JSValue* jsNumberCell(JSGlobalData*, double);
    JSValue* jsNaN(JSGlobalData*);
    JSValue* jsNumberCell(ExecState*, double);
    JSValue* jsNaN(ExecState*);

    ALWAYS_INLINE JSValue* jsNumber(ExecState* exec, double d)
    {
        JSValue* v = JSImmediate::from(d);
        return v ? v : jsNumberCell(exec, d);
    }

    ALWAYS_INLINE JSValue* jsNumber(ExecState* exec, short i)
    {
        JSValue* v = JSImmediate::from(i);
        return v ? v : jsNumberCell(exec, i);
    }

    ALWAYS_INLINE JSValue* jsNumber(ExecState* exec, unsigned short i)
    {
        JSValue* v = JSImmediate::from(i);
        return v ? v : jsNumberCell(exec, i);
    }

    ALWAYS_INLINE JSValue* jsNumber(ExecState* exec, int i)
    {
        JSValue* v = JSImmediate::from(i);
        return v ? v : jsNumberCell(exec, i);
    }

    ALWAYS_INLINE JSValue* jsNumber(ExecState* exec, unsigned i)
    {
        JSValue* v = JSImmediate::from(i);
        return v ? v : jsNumberCell(exec, i);
    }

    ALWAYS_INLINE JSValue* jsNumber(ExecState* exec, long i)
    {
        JSValue* v = JSImmediate::from(i);
        return v ? v : jsNumberCell(exec, i);
    }

    ALWAYS_INLINE JSValue* jsNumber(ExecState* exec, unsigned long i)
    {
        JSValue* v = JSImmediate::from(i);
        return v ? v : jsNumberCell(exec, i);
    }

    ALWAYS_INLINE JSValue* jsNumber(ExecState* exec, long long i)
    {
        JSValue* v = JSImmediate::from(i);
        return v ? v : jsNumberCell(exec, static_cast<double>(i));
    }

    ALWAYS_INLINE JSValue* jsNumber(ExecState* exec, unsigned long long i)
    {
        JSValue* v = JSImmediate::from(i);
        return v ? v : jsNumberCell(exec, static_cast<double>(i));
    }

    ALWAYS_INLINE JSValue* jsNumber(JSGlobalData* globalData, double d)
    {
        JSValue* v = JSImmediate::from(d);
        return v ? v : jsNumberCell(globalData, d);
    }

    ALWAYS_INLINE JSValue* jsNumber(JSGlobalData* globalData, short i)
    {
        JSValue* v = JSImmediate::from(i);
        return v ? v : jsNumberCell(globalData, i);
    }

    ALWAYS_INLINE JSValue* jsNumber(JSGlobalData* globalData, unsigned short i)
    {
        JSValue* v = JSImmediate::from(i);
        return v ? v : jsNumberCell(globalData, i);
    }

    ALWAYS_INLINE JSValue* jsNumber(JSGlobalData* globalData, int i)
    {
        JSValue* v = JSImmediate::from(i);
        return v ? v : jsNumberCell(globalData, i);
    }

    ALWAYS_INLINE JSValue* jsNumber(JSGlobalData* globalData, unsigned i)
    {
        JSValue* v = JSImmediate::from(i);
        return v ? v : jsNumberCell(globalData, i);
    }

    ALWAYS_INLINE JSValue* jsNumber(JSGlobalData* globalData, long i)
    {
        JSValue* v = JSImmediate::from(i);
        return v ? v : jsNumberCell(globalData, i);
    }

    ALWAYS_INLINE JSValue* jsNumber(JSGlobalData* globalData, unsigned long i)
    {
        JSValue* v = JSImmediate::from(i);
        return v ? v : jsNumberCell(globalData, i);
    }

    ALWAYS_INLINE JSValue* jsNumber(JSGlobalData* globalData, long long i)
    {
        JSValue* v = JSImmediate::from(i);
        return v ? v : jsNumberCell(globalData, static_cast<double>(i));
    }

    ALWAYS_INLINE JSValue* jsNumber(JSGlobalData* globalData, unsigned long long i)
    {
        JSValue* v = JSImmediate::from(i);
        return v ? v : jsNumberCell(globalData, static_cast<double>(i));
    }

    // --- JSValue inlines ----------------------------

    inline double JSValue::uncheckedGetNumber() const
    {
        ASSERT(JSImmediate::isImmediate(this) || asCell()->isNumber());
        return JSImmediate::isImmediate(this) ? JSImmediate::toDouble(this) : static_cast<const JSNumberCell*>(this)->value();
    }

    inline int32_t JSNumberCell::toInt32() const
    {
        if (m_value >= -2147483648.0 && m_value < 2147483648.0)
            return static_cast<int32_t>(m_value);
        bool scratch;
        return JSValue::toInt32SlowCase(m_value, scratch);
    }

    inline uint32_t JSNumberCell::toUInt32() const
    {
        if (m_value >= 0.0 && m_value < 4294967296.0)
            return static_cast<uint32_t>(m_value);
        bool scratch;
        return JSValue::toUInt32SlowCase(m_value, scratch);
    }

    ALWAYS_INLINE JSValue* JSValue::toJSNumber(ExecState* exec) const
    {
        return JSImmediate::isNumber(this) ? const_cast<JSValue*>(this) : jsNumber(exec, this->toNumber(exec));
    }

} // namespace JSC

#endif // JSNumberCell_h
