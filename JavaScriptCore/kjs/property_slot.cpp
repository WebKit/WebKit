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
 *  the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 *  Boston, MA 02110-1301, USA.
 *
 */


#include "config.h"
#include "property_slot.h"
#include "object.h"

namespace KJS {

JSValue *PropertySlot::undefinedGetter(ExecState*, JSObject*, const Identifier&, const PropertySlot&)
{
    return jsUndefined();
}

JSValue* PropertySlot::ungettableGetter(ExecState*, JSObject*, const Identifier&, const PropertySlot&)
{
    ASSERT_NOT_REACHED();
    return jsUndefined();
}

JSValue *PropertySlot::functionGetter(ExecState* exec, JSObject* originalObject, const Identifier&, const PropertySlot& slot)
{
    return slot.m_data.getterFunc->call(exec, originalObject, exec->emptyList());
}

}
