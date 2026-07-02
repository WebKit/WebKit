/*
 *  Copyright (C) 1999-2000 Harri Porten (porten@kde.org)
 *  Copyright (C) 2008-2022 Apple Inc. All rights reserved.
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

#pragma once

#include "ErrorInstance.h"
#include "StructureCreateInlines.h"

namespace JSC {

inline Structure* ErrorInstance::createStructure(VM& vm, JSGlobalObject* globalObject, JSValue prototype)
{
    return Structure::create(vm, globalObject, prototype, TypeInfo(ErrorInstanceType, StructureFlags), info());
}

inline ErrorInstance* ErrorInstance::create(VM& vm, Structure* structure, const String& message, JSValue cause, SourceAppender appender, RuntimeType type, ErrorType errorType, bool useCurrentFrame, JSCell* subclassCaller)
{
    ErrorInstance* instance = new (NotNull, allocateCell<ErrorInstance>(vm)) ErrorInstance(vm, structure, errorType);
    instance->finishCreation(vm, message, cause, appender, type, useCurrentFrame, subclassCaller);
    return instance;
}

inline ErrorInstance* ErrorInstance::create(VM& vm, Structure* structure, const String& message, JSValue cause, ErrorType errorType, JSCell* owner, CallLinkInfo* callLinkInfo)
{
    ErrorInstance* instance = new (NotNull, allocateCell<ErrorInstance>(vm)) ErrorInstance(vm, structure, errorType);
    instance->finishCreation(vm, message, cause, owner, callLinkInfo);
    return instance;
}

} // namespace JSC
