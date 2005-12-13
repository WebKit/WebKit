// -*- c-basic-offset: 4 -*-
/*
 *  This file is part of the KDE libraries
 *  Copyright (C) 2005 Apple Computer, Inc.
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
 *  the Free Software Foundation, Inc., 51 Franklin Steet, Fifth Floor,
 *  Boston, MA 02110-1301, USA.
 *
 */

#ifndef KJS_PROPERTY_SLOT_H
#define KJS_PROPERTY_SLOT_H

#include "identifier.h"
#include "value.h"

namespace KJS {

class HashEntry;
class ExecState;
class JSObject;

#define VALUE_SLOT_MARKER ((GetValueFunc)1)
class PropertySlot
{
public:
    typedef JSValue *(*GetValueFunc)(ExecState *, JSObject *originalObject, const Identifier&, const PropertySlot&);

    JSValue *getValue(ExecState *exec, JSObject *originalObject, const Identifier& propertyName) const
    { 
        if (m_getValue == VALUE_SLOT_MARKER)
            return *m_data.valueSlot;
        return m_getValue(exec, originalObject, propertyName, *this); 
    }

    JSValue *getValue(ExecState *exec, JSObject *originalObject, unsigned propertyName) const
    { 
        if (m_getValue == VALUE_SLOT_MARKER)
            return *m_data.valueSlot;
        return m_getValue(exec, originalObject, Identifier::from(propertyName), *this); 
    }
    
    void setValueSlot(JSObject *slotBase, JSValue **valueSlot) 
    {
        m_slotBase = slotBase;
        m_data.valueSlot = valueSlot;
        m_getValue = VALUE_SLOT_MARKER;
    }

    void setStaticEntry(JSObject *slotBase, const HashEntry *staticEntry, GetValueFunc getValue)
    {
        assert(getValue);
        m_slotBase = slotBase;
        m_data.staticEntry = staticEntry;
        m_getValue = getValue;
    }

    void setCustom(JSObject *slotBase, GetValueFunc getValue)
    {
        assert(getValue);
        m_slotBase = slotBase;
        m_getValue = getValue;
    }

    void setCustomIndex(JSObject *slotBase, unsigned index, GetValueFunc getValue)
    {
        assert(getValue);
        m_slotBase = slotBase;
        m_data.index = index;
        m_getValue = getValue;
    }
    
    void setGetterSlot(JSObject *slotBase, JSObject *getterFunc)
    {
        m_getValue = functionGetter;
        m_slotBase = slotBase;
        m_data.getterFunc = getterFunc;
    }
    
    void setUndefined(JSObject *slotBase)
    {
        m_slotBase = slotBase;
        m_getValue = undefinedGetter;
    }

    JSObject *slotBase() const { return m_slotBase; }

    const HashEntry *staticEntry() const { return m_data.staticEntry; }
    unsigned index() const { return m_data.index; }

private:
    static JSValue *undefinedGetter(ExecState *, JSObject *, const Identifier&, const PropertySlot&);
    static JSValue *functionGetter(ExecState *, JSObject *, const Identifier&, const PropertySlot&);
    
    GetValueFunc m_getValue;

    JSObject *m_slotBase;
    union {
        JSObject *getterFunc;
        JSValue **valueSlot;
        const HashEntry *staticEntry;
        unsigned index;
    } m_data;
};

}

#endif // KJS_PROPERTY_SLOT_H
