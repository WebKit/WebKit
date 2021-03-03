/*
 * Copyright (C) 2016 Apple Inc. All rights reserved.
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
#include "JSWebAssemblyCompileError.h"

#if ENABLE(WEBASSEMBLY)

#include "JSCellInlines.h"

namespace JSC {

JSWebAssemblyCompileError* JSWebAssemblyCompileError::create(JSGlobalObject* globalObject, VM& vm, Structure* structure, const String& message)
{
    auto* instance = new (NotNull, allocateCell<JSWebAssemblyCompileError>(vm.heap)) JSWebAssemblyCompileError(vm, structure);
    bool useCurrentFrame = true;
    instance->finishCreation(vm, globalObject, message, defaultSourceAppender, TypeNothing, useCurrentFrame);
    return instance;
}

JSWebAssemblyCompileError::JSWebAssemblyCompileError(VM& vm, Structure* structure)
    : Base(vm, structure, ErrorType::Error)
{
}

const ClassInfo JSWebAssemblyCompileError::s_info = { "WebAssembly.CompileError", &Base::s_info, nullptr, nullptr, CREATE_METHOD_TABLE(JSWebAssemblyCompileError) };

    
JSObject* createJSWebAssemblyCompileError(JSGlobalObject* globalObject, VM& vm, const String& message)
{
    ASSERT(!message.isEmpty());
    return JSWebAssemblyCompileError::create(globalObject, vm, globalObject->webAssemblyCompileErrorStructure(), message);
}

} // namespace JSC

#endif // ENABLE(WEBASSEMBLY)
