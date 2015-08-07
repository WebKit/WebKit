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

#ifndef JSWASMModule_h
#define JSWASMModule_h

#if ENABLE(WEBASSEMBLY)

#include "JSDestructibleObject.h"
#include "WASMFormat.h"

namespace JSC {

class JSWASMModule : public JSDestructibleObject {
public:
    typedef JSDestructibleObject Base;

    static JSWASMModule* create(VM& vm, Structure* structure)
    {
        JSWASMModule* module = new (NotNull, allocateCell<JSWASMModule>(vm.heap)) JSWASMModule(vm, structure);
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
    Vector<WASMSignature>& signatures() { return m_signatures; }
    Vector<WASMFunctionImport>& functionImports() { return m_functionImports; }
    Vector<WASMFunctionImportSignature>& functionImportSignatures() { return m_functionImportSignatures; }
    Vector<WASMType>& globalVariableTypes() { return m_globalVariableTypes; }
    Vector<WASMFunctionDeclaration>& functionDeclarations() { return m_functionDeclarations; }
    Vector<WASMFunctionPointerTable>& functionPointerTables() { return m_functionPointerTables; }

private:
    JSWASMModule(VM& vm, Structure* structure)
        : Base(vm, structure)
    {
    }

    Vector<uint32_t> m_i32Constants;
    Vector<float> m_f32Constants;
    Vector<double> m_f64Constants;
    Vector<WASMSignature> m_signatures;
    Vector<WASMFunctionImport> m_functionImports;
    Vector<WASMFunctionImportSignature> m_functionImportSignatures;
    Vector<WASMType> m_globalVariableTypes;
    Vector<WASMFunctionDeclaration> m_functionDeclarations;
    Vector<WASMFunctionPointerTable> m_functionPointerTables;

    Vector<WriteBarrier<JSFunction>> m_functions;
};

} // namespace JSC

#endif // ENABLE(WEBASSEMBLY)

#endif // JSWASMModule_h
