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

#include "CallFrame.h"
#include "JSCell.h"
#include "JSImmediate.h"
#include "Collector.h"
#include "UString.h"
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
        friend class JIT;
        friend JSValuePtr jsNumberCell(JSGlobalData*, double);
        friend JSValuePtr jsNaN(JSGlobalData*);
        friend JSValuePtr jsNumberCell(ExecState*, double);
        friend JSValuePtr jsNaN(ExecState*);
    public:
        double value() const { return m_value; }

        virtual JSValuePtr toPrimitive(ExecState*, PreferredPrimitiveType) const;
        virtual bool getPrimitiveNumber(ExecState*, double& number, JSValuePtr& value);
        virtual bool toBoolean(ExecState*) const;
        virtual double toNumber(ExecState*) const;
        virtual UString toString(ExecState*) const;
        virtual JSObject* toObject(ExecState*) const;

        virtual UString toThisString(ExecState*) const;
        virtual JSObject* toThisObject(ExecState*) const;
        virtual JSValuePtr getJSNumber();

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

        static PassRefPtr<Structure> createStructure(JSValuePtr proto) { return Structure::create(proto, TypeInfo(NumberType, NeedsThisConversion)); }

    private:
        JSNumberCell(JSGlobalData* globalData, double value)
            : JSCell(globalData->numberStructure.get())
            , m_value(value)
        {
        }

        JSNumberCell(ExecState* exec, double value)
            : JSCell(exec->globalData().numberStructure.get())
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

    JSNumberCell* asNumberCell(JSValuePtr);

    JSValuePtr jsNumberCell(JSGlobalData*, double);
    JSValuePtr jsNaN(JSGlobalData*);
    JSValuePtr jsNumberCell(ExecState*, double);
    JSValuePtr jsNaN(ExecState*);

    inline JSNumberCell* asNumberCell(JSValuePtr value)
    {
        ASSERT(asCell(value)->isNumber());
        return static_cast<JSNumberCell*>(asCell(value));
    }

    ALWAYS_INLINE JSValuePtr jsNumber(ExecState* exec, double d)
    {
        JSValuePtr v = JSImmediate::from(d);
        return v ? v : jsNumberCell(exec, d);
    }

    ALWAYS_INLINE JSValuePtr jsNumber(ExecState* exec, short i)
    {
        JSValuePtr v = JSImmediate::from(i);
        return v ? v : jsNumberCell(exec, i);
    }

    ALWAYS_INLINE JSValuePtr jsNumber(ExecState* exec, unsigned short i)
    {
        JSValuePtr v = JSImmediate::from(i);
        return v ? v : jsNumberCell(exec, i);
    }

    ALWAYS_INLINE JSValuePtr jsNumber(ExecState* exec, int i)
    {
        JSValuePtr v = JSImmediate::from(i);
        return v ? v : jsNumberCell(exec, i);
    }

    ALWAYS_INLINE JSValuePtr jsNumber(ExecState* exec, unsigned i)
    {
        JSValuePtr v = JSImmediate::from(i);
        return v ? v : jsNumberCell(exec, i);
    }

    ALWAYS_INLINE JSValuePtr jsNumber(ExecState* exec, long i)
    {
        JSValuePtr v = JSImmediate::from(i);
        return v ? v : jsNumberCell(exec, i);
    }

    ALWAYS_INLINE JSValuePtr jsNumber(ExecState* exec, unsigned long i)
    {
        JSValuePtr v = JSImmediate::from(i);
        return v ? v : jsNumberCell(exec, i);
    }

    ALWAYS_INLINE JSValuePtr jsNumber(ExecState* exec, long long i)
    {
        JSValuePtr v = JSImmediate::from(i);
        return v ? v : jsNumberCell(exec, static_cast<double>(i));
    }

    ALWAYS_INLINE JSValuePtr jsNumber(ExecState* exec, unsigned long long i)
    {
        JSValuePtr v = JSImmediate::from(i);
        return v ? v : jsNumberCell(exec, static_cast<double>(i));
    }

    ALWAYS_INLINE JSValuePtr jsNumber(JSGlobalData* globalData, double d)
    {
        JSValuePtr v = JSImmediate::from(d);
        return v ? v : jsNumberCell(globalData, d);
    }

    ALWAYS_INLINE JSValuePtr jsNumber(JSGlobalData* globalData, short i)
    {
        JSValuePtr v = JSImmediate::from(i);
        return v ? v : jsNumberCell(globalData, i);
    }

    ALWAYS_INLINE JSValuePtr jsNumber(JSGlobalData* globalData, unsigned short i)
    {
        JSValuePtr v = JSImmediate::from(i);
        return v ? v : jsNumberCell(globalData, i);
    }

    ALWAYS_INLINE JSValuePtr jsNumber(JSGlobalData* globalData, int i)
    {
        JSValuePtr v = JSImmediate::from(i);
        return v ? v : jsNumberCell(globalData, i);
    }

    ALWAYS_INLINE JSValuePtr jsNumber(JSGlobalData* globalData, unsigned i)
    {
        JSValuePtr v = JSImmediate::from(i);
        return v ? v : jsNumberCell(globalData, i);
    }

    ALWAYS_INLINE JSValuePtr jsNumber(JSGlobalData* globalData, long i)
    {
        JSValuePtr v = JSImmediate::from(i);
        return v ? v : jsNumberCell(globalData, i);
    }

    ALWAYS_INLINE JSValuePtr jsNumber(JSGlobalData* globalData, unsigned long i)
    {
        JSValuePtr v = JSImmediate::from(i);
        return v ? v : jsNumberCell(globalData, i);
    }

    ALWAYS_INLINE JSValuePtr jsNumber(JSGlobalData* globalData, long long i)
    {
        JSValuePtr v = JSImmediate::from(i);
        return v ? v : jsNumberCell(globalData, static_cast<double>(i));
    }

    ALWAYS_INLINE JSValuePtr jsNumber(JSGlobalData* globalData, unsigned long long i)
    {
        JSValuePtr v = JSImmediate::from(i);
        return v ? v : jsNumberCell(globalData, static_cast<double>(i));
    }

    // --- JSValue inlines ----------------------------

    inline double JSValuePtr::uncheckedGetNumber() const
    {
        ASSERT(JSImmediate::isImmediate(asValue()) || asCell()->isNumber());
        return JSImmediate::isImmediate(asValue()) ? JSImmediate::toDouble(asValue()) : asNumberCell(asValue())->value();
    }

    inline int32_t JSNumberCell::toInt32() const
    {
        if (m_value >= -2147483648.0 && m_value < 2147483648.0)
            return static_cast<int32_t>(m_value);
        bool scratch;
        return JSC::toInt32SlowCase(m_value, scratch);
    }

    inline uint32_t JSNumberCell::toUInt32() const
    {
        if (m_value >= 0.0 && m_value < 4294967296.0)
            return static_cast<uint32_t>(m_value);
        bool scratch;
        return JSC::toUInt32SlowCase(m_value, scratch);
    }

    ALWAYS_INLINE JSValuePtr JSValuePtr::toJSNumber(ExecState* exec) const
    {
        return JSImmediate::isNumber(asValue()) ? asValue() : jsNumber(exec, this->toNumber(exec));
    }

} // namespace JSC

#endif // JSNumberCell_h
