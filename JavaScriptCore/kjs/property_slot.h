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

#ifndef _KJS_PROPERTY_SLOT_H_
#define _KJS_PROPERTY_SLOT_H_

#include "identifier.h"
#include "value.h"

namespace KJS {

class HashEntry;
class ExecState;
class ObjectImp;

#define VALUE_SLOT_MARKER ((GetValueFunc)1)
class PropertySlot
{
public:
    typedef ValueImp *(*GetValueFunc)(ExecState *, const Identifier&, const PropertySlot&);

    bool isSet() const { return m_getValue != 0; }

    ValueImp *getValue(ExecState *exec, const Identifier& propertyName) const
    { 
        if (m_getValue == VALUE_SLOT_MARKER)
            return *m_data.valueSlot;
        return m_getValue(exec, propertyName, *this); 
    }

    ValueImp *getValue(ExecState *exec, unsigned propertyName) const
    { 
        if (m_getValue == VALUE_SLOT_MARKER)
            return *m_data.valueSlot;
        return m_getValue(exec, Identifier::from(propertyName), *this); 
    }
    
    void setValueSlot(ObjectImp *slotBase, ValueImp **valueSlot) 
    {
        m_slotBase = slotBase;
        m_data.valueSlot = valueSlot;
        m_getValue = VALUE_SLOT_MARKER;
    }

    void setStaticEntry(ObjectImp *slotBase, const HashEntry *staticEntry, GetValueFunc getValue)
    {
        m_slotBase = slotBase;
        m_data.staticEntry = staticEntry;
        m_getValue = getValue;
    }

    void setCustom(ObjectImp *slotBase, GetValueFunc getValue)
    {
        m_slotBase = slotBase;
        m_getValue = getValue;
    }

    void setCustomIndex(ObjectImp *slotBase, unsigned long index, GetValueFunc getValue)
    {
        m_slotBase = slotBase;
        m_data.index = index;
        m_getValue = getValue;
    }

    void setUndefined(ObjectImp *slotBase)
    {
        m_slotBase = slotBase;
        m_getValue = undefinedGetter;
    }

    ObjectImp *slotBase() const { return m_slotBase; }

    const HashEntry *staticEntry() const { return m_data.staticEntry; }
    unsigned long index() const { return m_data.index; }

private:
    static ValueImp *undefinedGetter(ExecState *, const Identifier&, const PropertySlot&);

    GetValueFunc m_getValue;

    ObjectImp *m_slotBase;
    union {
        ValueImp **valueSlot;
        const HashEntry *staticEntry;
        unsigned long index;
    } m_data;
};

}

#endif _KJS_PROPERTY_SLOT_H_
