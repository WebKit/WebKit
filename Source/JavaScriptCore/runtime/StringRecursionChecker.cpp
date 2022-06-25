/*
 *  Copyright (C) 2011-2019 Apple Inc. All rights reserved.
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
#include "StringRecursionChecker.h"

#include "JSCJSValueInlines.h"
#include "JSGlobalObject.h"

namespace JSC {

JSValue StringRecursionChecker::throwStackOverflowError()
{
    VM& vm = m_globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    return JSC::throwStackOverflowError(m_globalObject, scope);
}

JSValue StringRecursionChecker::emptyString()
{
    return jsEmptyString(m_globalObject->vm());
}

}
