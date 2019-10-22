/*
 * Copyright (C) 2012-2019 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#include "config.h"
#include "ProfilerOSRExit.h"

#include "JSGlobalObject.h"
#include "ObjectConstructor.h"
#include "JSCInlines.h"

namespace JSC { namespace Profiler {

OSRExit::OSRExit(unsigned id, const OriginStack& origin, ExitKind kind, bool isWatchpoint)
    : m_origin(origin)
    , m_id(id)
    , m_exitKind(kind)
    , m_isWatchpoint(isWatchpoint)
    , m_counter(0)
{
}

OSRExit::~OSRExit()
{
}

JSValue OSRExit::toJS(JSGlobalObject* globalObject) const
{
    VM& vm = globalObject->vm();
    JSObject* result = constructEmptyObject(globalObject);
    result->putDirect(vm, vm.propertyNames->id, jsNumber(m_id));
    result->putDirect(vm, vm.propertyNames->origin, m_origin.toJS(globalObject));
    result->putDirect(vm, vm.propertyNames->exitKind, jsString(vm, exitKindToString(m_exitKind)));
    result->putDirect(vm, vm.propertyNames->isWatchpoint, jsBoolean(m_isWatchpoint));
    result->putDirect(vm, vm.propertyNames->count, jsNumber(m_counter));
    return result;
}

} } // namespace JSC::Profiler

