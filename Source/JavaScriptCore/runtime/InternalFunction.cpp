/*
 *  Copyright (C) 1999-2002 Harri Porten (porten@kde.org)
 *  Copyright (C) 2001 Peter Kelly (pmk@post.com)
 *  Copyright (C) 2004, 2007, 2008 Apple Inc. All rights reserved.
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
#include "InternalFunction.h"

#include "FunctionPrototype.h"
#include "JSGlobalObject.h"
#include "JSString.h"

namespace JSC {

ASSERT_CLASS_FITS_IN_CELL(InternalFunction);
ASSERT_HAS_TRIVIAL_DESTRUCTOR(InternalFunction);

const ClassInfo InternalFunction::s_info = { "Function", &JSNonFinalObject::s_info, 0, 0, CREATE_METHOD_TABLE(InternalFunction) };

InternalFunction::InternalFunction(JSGlobalObject* globalObject, Structure* structure)
    : JSNonFinalObject(globalObject->globalData(), structure)
{
}

void InternalFunction::finishCreation(JSGlobalData& globalData, const Identifier& name)
{
    Base::finishCreation(globalData);
    ASSERT(inherits(&s_info));
    ASSERT(methodTable()->getCallData != InternalFunction::s_info.methodTable.getCallData);
    putDirect(globalData, globalData.propertyNames->name, jsString(&globalData, name.isNull() ? "" : name.ustring()), DontDelete | ReadOnly | DontEnum);
}

const UString& InternalFunction::name(ExecState* exec)
{
    return asString(getDirect(exec->globalData(), exec->globalData().propertyNames->name))->tryGetValue();
}

const UString InternalFunction::displayName(ExecState* exec)
{
    JSValue displayName = getDirect(exec->globalData(), exec->globalData().propertyNames->displayName);
    
    if (displayName && isJSString(displayName))
        return asString(displayName)->tryGetValue();
    
    return UString();
}

CallType InternalFunction::getCallData(JSCell*, CallData&)
{
    ASSERT_NOT_REACHED();
    return CallTypeNone;
}

const UString InternalFunction::calculatedDisplayName(ExecState* exec)
{
    const UString explicitName = displayName(exec);
    
    if (!explicitName.isEmpty())
        return explicitName;
    
    return name(exec);
}

} // namespace JSC
