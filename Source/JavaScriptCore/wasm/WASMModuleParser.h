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

#ifndef WASMModuleParser_h
#define WASMModuleParser_h

#if ENABLE(WEBASSEMBLY)

#include "Strong.h"
#include "WASMReader.h"
#include <wtf/text/WTFString.h>

namespace JSC {

class ExecState;
class JSArrayBuffer;
class JSGlobalObject;
class JSWASMModule;
class SourceCode;
class VM;

class WASMModuleParser {
public:
    WASMModuleParser(VM&, JSGlobalObject*, const SourceCode&, JSObject* imports, JSArrayBuffer*);
    JSWASMModule* parse(ExecState*, String& errorMessage);

private:
    void parseModule(ExecState*);
    void parseConstantPoolSection();
    void parseSignatureSection();
    void parseFunctionImportSection(ExecState*);
    void parseGlobalSection(ExecState*);
    void parseFunctionDeclarationSection();
    void parseFunctionPointerTableSection();
    void parseFunctionDefinitionSection();
    void parseFunctionDefinition(size_t functionIndex);
    void parseExportSection();
    void getImportedValue(ExecState*, const String& importName, JSValue&);

    VM& m_vm;
    Strong<JSGlobalObject> m_globalObject;
    const SourceCode& m_source;
    Strong<JSObject> m_imports;
    WASMReader m_reader;
    Strong<JSWASMModule> m_module;
    String m_errorMessage;
};

JS_EXPORT_PRIVATE JSWASMModule* parseWebAssembly(ExecState*, const SourceCode&, JSObject* imports, JSArrayBuffer*, String& errorMessage);

} // namespace JSC

#endif // ENABLE(WEBASSEMBLY)

#endif // WASMModuleParser_h
