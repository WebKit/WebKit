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
#include "WebAssemblyFunctionCell.h"

#if ENABLE(WEBASSEMBLY)

#include "JSCInlines.h"

namespace JSC {

const ClassInfo WebAssemblyFunctionCell::s_info = { "WebAssemblyFunctionCell", nullptr, nullptr, CREATE_METHOD_TABLE(WebAssemblyFunctionCell) };

WebAssemblyFunctionCell* WebAssemblyFunctionCell::create(VM& vm, CallableWebAssemblyFunction&& callable)
{
    WebAssemblyFunctionCell* nativeFunction = new (NotNull, allocateCell<WebAssemblyFunctionCell>(vm.heap)) WebAssemblyFunctionCell(vm, WTFMove(callable));
    nativeFunction->finishCreation(vm);
    return nativeFunction;
}

WebAssemblyFunctionCell::WebAssemblyFunctionCell(VM& vm, CallableWebAssemblyFunction&& callable)
    : Base(vm, vm.webAssemblyFunctionCellStructure.get())
    , m_function(WTFMove(callable))
{
}

void WebAssemblyFunctionCell::destroy(JSCell* cell)
{
    WebAssemblyFunctionCell* nativeFunction = static_cast<WebAssemblyFunctionCell*>(cell);
    nativeFunction->WebAssemblyFunctionCell::~WebAssemblyFunctionCell();
}

Structure* WebAssemblyFunctionCell::createStructure(VM& vm, JSGlobalObject* globalObject, JSValue prototype)
{
    return Structure::create(vm, globalObject, prototype, TypeInfo(CellType, StructureFlags), info());
}

}

#endif // ENABLE(WEBASSEMBLY)
