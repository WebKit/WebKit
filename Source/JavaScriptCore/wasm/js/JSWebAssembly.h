/*
 * Copyright (C) 2016-2017 Apple Inc. All rights reserved.
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

#pragma once

#if ENABLE(WEBASSEMBLY)

#include "JSObject.h"
#include "JSWebAssemblyCompileError.h"
#include "JSWebAssemblyInstance.h"
#include "JSWebAssemblyLinkError.h"
#include "JSWebAssemblyMemory.h"
#include "JSWebAssemblyModule.h"
#include "JSWebAssemblyRuntimeError.h"
#include "JSWebAssemblyTable.h"
#include "WebAssemblyCompileErrorConstructor.h"
#include "WebAssemblyCompileErrorPrototype.h"
#include "WebAssemblyFunction.h"
#include "WebAssemblyInstanceConstructor.h"
#include "WebAssemblyInstancePrototype.h"
#include "WebAssemblyLinkErrorConstructor.h"
#include "WebAssemblyLinkErrorPrototype.h"
#include "WebAssemblyMemoryConstructor.h"
#include "WebAssemblyMemoryPrototype.h"
#include "WebAssemblyModuleConstructor.h"
#include "WebAssemblyModulePrototype.h"
#include "WebAssemblyModuleRecord.h"
#include "WebAssemblyPrototype.h"
#include "WebAssemblyRuntimeErrorConstructor.h"
#include "WebAssemblyRuntimeErrorPrototype.h"
#include "WebAssemblyTableConstructor.h"
#include "WebAssemblyTablePrototype.h"
#include "WebAssemblyToJSCallee.h"

namespace JSC {

class JSWebAssembly final : public JSNonFinalObject {
public:
    using Base = JSNonFinalObject;
    static const unsigned StructureFlags = Base::StructureFlags | HasStaticPropertyTable;

    static JSWebAssembly* create(VM&, JSGlobalObject*, Structure*);
    static Structure* createStructure(VM&, JSGlobalObject*, JSValue);

    DECLARE_INFO;

protected:
    void finishCreation(VM&);

private:
    JSWebAssembly(VM&, Structure*);
};

} // namespace JSC

#endif // ENABLE(WEBASSEMBLY)
