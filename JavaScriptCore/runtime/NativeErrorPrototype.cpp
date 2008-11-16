/*
 *  Copyright (C) 1999-2000 Harri Porten (porten@kde.org)
 *  Copyright (C) 2003, 2008 Apple Inc. All rights reserved.
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

#include "config.h"
#include "NativeErrorPrototype.h"

#include "ErrorPrototype.h"
#include "JSString.h"
#include "UString.h"

namespace JSC {

ASSERT_CLASS_FITS_IN_CELL(NativeErrorPrototype);

NativeErrorPrototype::NativeErrorPrototype(ExecState* exec, PassRefPtr<Structure> structure, const UString& name, const UString& message)
    : JSObject(structure)
{
    putDirect(exec->propertyNames().name, jsString(exec, name), 0);
    putDirect(exec->propertyNames().message, jsString(exec, message), 0);
}

} // namespace JSC
