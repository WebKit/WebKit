/*
 * Copyright (C) 2015 Apple Inc. All rights reserved.
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

#include "JSDestructibleObject.h"
#include "WasmFormat.h"

namespace JSC {

class JSWasmModule : public JSDestructibleObject {
public:
    typedef JSDestructibleObject Base;

    union GlobalVariable {
        GlobalVariable(int32_t value)
            : intValue(value)
        {
        }
        GlobalVariable(float value)
            : floatValue(value)
        {
        }
        GlobalVariable(double value)
            : doubleValue(value)
        {
        }

        int32_t intValue;
        float floatValue;
        double doubleValue;
    };

    static JSWasmModule* create(VM& vm, Structure* structure, JSArrayBuffer* arrayBuffer)
    {
        JSWasmModule* module = new (NotNull, allocateCell<JSWasmModule>(vm.heap)) JSWasmModule(vm, structure, arrayBuffer);
        module->finishCreation(vm);
        return module;
    }

    DECLARE_INFO;

    static Structure* createStructure(VM& vm, JSGlobalObject* globalObject)
    {
        return Structure::create(vm, globalObject, jsNull(), TypeInfo(ObjectType, StructureFlags), info());
    }

    static void destroy(JSCell*);
    static void visitChildren(JSCell*, SlotVisitor&);

    Vector<uint32_t>& i32Constants() { return m_i32Constants; }
    Vector<float>& f32Constants() { return m_f32Constants; }
    Vector<double>& f64Constants() { return m_f64Constants; }
    Vector<Wasm::Signature>& signatures() { return m_signatures; }
    Vector<Wasm::FunctionImport>& functionImports() { return m_functionImports; }
    Vector<Wasm::FunctionImportSignature>& functionImportSignatures() { return m_functionImportSignatures; }
    Vector<Wasm::Type>& globalVariableTypes() { return m_globalVariableTypes; }
    Vector<Wasm::FunctionDeclaration>& functionDeclarations() { return m_functionDeclarations; }
    Vector<Wasm::FunctionPointerTable>& functionPointerTables() { return m_functionPointerTables; }

    const JSArrayBuffer* arrayBuffer() const { return m_arrayBuffer.get(); }
    Vector<WriteBarrier<JSFunction>>& functions() { return m_functions; }
    Vector<unsigned>& functionStartOffsetsInSource() { return m_functionStartOffsetsInSource; }
    Vector<unsigned>& functionStackHeights() { return m_functionStackHeights; }
    Vector<GlobalVariable>& globalVariables() { return m_globalVariables; }
    Vector<WriteBarrier<JSFunction>>& importedFunctions() { return m_importedFunctions; }

private:
    JSWasmModule(VM&, Structure*, JSArrayBuffer*);

    Vector<uint32_t> m_i32Constants;
    Vector<float> m_f32Constants;
    Vector<double> m_f64Constants;
    Vector<Wasm::Signature> m_signatures;
    Vector<Wasm::FunctionImport> m_functionImports;
    Vector<Wasm::FunctionImportSignature> m_functionImportSignatures;
    Vector<Wasm::Type> m_globalVariableTypes;
    Vector<Wasm::FunctionDeclaration> m_functionDeclarations;
    Vector<Wasm::FunctionPointerTable> m_functionPointerTables;

    WriteBarrier<JSArrayBuffer> m_arrayBuffer;
    Vector<WriteBarrier<JSFunction>> m_functions;
    Vector<unsigned> m_functionStartOffsetsInSource;
    Vector<unsigned> m_functionStackHeights;
    Vector<GlobalVariable> m_globalVariables;
    Vector<WriteBarrier<JSFunction>> m_importedFunctions;
};

} // namespace JSC

#endif // ENABLE(WEBASSEMBLY)
