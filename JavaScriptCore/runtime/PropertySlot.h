/*
 *  Copyright (C) 2005, 2007, 2008 Apple Inc. All rights reserved.
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

#ifndef PropertySlot_h
#define PropertySlot_h

#include "Identifier.h"
#include "JSValue.h"
#include "JSImmediate.h"
#include "Register.h"
#include <wtf/Assertions.h>
#include <wtf/NotFound.h>

namespace JSC {

    class ExecState;
    class JSObject;

#define JSC_VALUE_SLOT_MARKER 0
#define JSC_REGISTER_SLOT_MARKER reinterpret_cast<GetValueFunc>(1)

    class PropertySlot {
    public:
        PropertySlot()
            : m_offset(WTF::notFound)
        {
            clearBase();
            clearValue();
        }

        explicit PropertySlot(const JSValuePtr base)
            : m_slotBase(base)
            , m_offset(WTF::notFound)
        {
            clearValue();
        }

        typedef JSValuePtr (*GetValueFunc)(ExecState*, const Identifier&, const PropertySlot&);

        JSValuePtr getValue(ExecState* exec, const Identifier& propertyName) const
        {
            if (m_getValue == JSC_VALUE_SLOT_MARKER)
                return *m_data.valueSlot;
            if (m_getValue == JSC_REGISTER_SLOT_MARKER)
                return (*m_data.registerSlot).jsValue(exec);
            return m_getValue(exec, propertyName, *this);
        }

        JSValuePtr getValue(ExecState* exec, unsigned propertyName) const
        {
            if (m_getValue == JSC_VALUE_SLOT_MARKER)
                return *m_data.valueSlot;
            if (m_getValue == JSC_REGISTER_SLOT_MARKER)
                return (*m_data.registerSlot).jsValue(exec);
            return m_getValue(exec, Identifier::from(exec, propertyName), *this);
        }

        bool isCacheable() const { return m_offset != WTF::notFound; }
        size_t cachedOffset() const
        {
            ASSERT(isCacheable());
            return m_offset;
        }

        void putValue(JSValuePtr value)
        { 
            if (m_getValue == JSC_VALUE_SLOT_MARKER) {
                *m_data.valueSlot = value;
                return;
            }
            ASSERT(m_getValue == JSC_REGISTER_SLOT_MARKER);
            *m_data.registerSlot = JSValuePtr(value);
        }

        void setValueSlot(JSValuePtr* valueSlot) 
        {
            ASSERT(valueSlot);
            m_getValue = JSC_VALUE_SLOT_MARKER;
            clearBase();
            m_data.valueSlot = valueSlot;
        }
        
        void setValueSlot(JSValuePtr slotBase, JSValuePtr* valueSlot)
        {
            ASSERT(valueSlot);
            m_getValue = JSC_VALUE_SLOT_MARKER;
            m_slotBase = slotBase;
            m_data.valueSlot = valueSlot;
        }
        
        void setValueSlot(JSValuePtr slotBase, JSValuePtr* valueSlot, size_t offset)
        {
            ASSERT(valueSlot);
            m_getValue = JSC_VALUE_SLOT_MARKER;
            m_slotBase = slotBase;
            m_data.valueSlot = valueSlot;
            m_offset = offset;
        }
        
        void setValue(JSValuePtr value)
        {
            ASSERT(value);
            m_getValue = JSC_VALUE_SLOT_MARKER;
            clearBase();
            m_value = value;
            m_data.valueSlot = &m_value;
        }

        void setRegisterSlot(Register* registerSlot)
        {
            ASSERT(registerSlot);
            m_getValue = JSC_REGISTER_SLOT_MARKER;
            clearBase();
            m_data.registerSlot = registerSlot;
        }

        void setCustom(JSValuePtr slotBase, GetValueFunc getValue)
        {
            ASSERT(slotBase);
            ASSERT(getValue);
            m_getValue = getValue;
            m_slotBase = slotBase;
        }

        void setCustomIndex(JSValuePtr slotBase, unsigned index, GetValueFunc getValue)
        {
            ASSERT(slotBase);
            ASSERT(getValue);
            m_getValue = getValue;
            m_slotBase = slotBase;
            m_data.index = index;
        }
        
        void setGetterSlot(JSObject* getterFunc)
        {
            ASSERT(getterFunc);
            m_getValue = functionGetter;
            m_data.getterFunc = getterFunc;
        }
        
        void setUndefined()
        {
            clearBase();
            setValue(jsUndefined());
        }

        JSValuePtr slotBase() const
        {
            ASSERT(m_slotBase);
            return m_slotBase;
        }

        void setBase(JSValuePtr base)
        {
            ASSERT(m_slotBase);
            ASSERT(base);
            m_slotBase = base;
        }

        void clearBase()
        {
#ifndef NDEBUG
            m_slotBase = noValue();
#endif
        }

        void clearValue()
        {
#ifndef NDEBUG
            m_value = noValue();
#endif
        }

        unsigned index() const { return m_data.index; }

    private:
        static JSValuePtr functionGetter(ExecState*, const Identifier&, const PropertySlot&);

        GetValueFunc m_getValue;
        
        JSValuePtr m_slotBase;
        union {
            JSObject* getterFunc;
            JSValuePtr* valueSlot;
            Register* registerSlot;
            unsigned index;
        } m_data;

        JSValuePtr m_value;

        size_t m_offset;
    };

} // namespace JSC

#endif // PropertySlot_h
