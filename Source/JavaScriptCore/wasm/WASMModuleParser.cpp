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

#include "config.h"
#include "WASMModuleParser.h"

#if ENABLE(WEBASSEMBLY)

#include "JSCInlines.h"
#include "JSWASMModule.h"
#include "WASMMagicNumber.h"

#define FAIL_WITH_MESSAGE(errorMessage) do { m_errorMessage = errorMessage; return false; } while (0)
#define READ_UNSIGNED_INT32_OR_FAIL(x, errorMessage) do { if (!m_reader.readUnsignedInt32(x)) FAIL_WITH_MESSAGE(errorMessage); } while (0)
#define READ_FLOAT_OR_FAIL(x, errorMessage) do { if (!m_reader.readFloat(x)) FAIL_WITH_MESSAGE(errorMessage); } while (0)
#define READ_DOUBLE_OR_FAIL(x, errorMessage) do { if (!m_reader.readDouble(x)) FAIL_WITH_MESSAGE(errorMessage); } while (0)
#define FAIL_IF_FALSE(condition, errorMessage) do { if (!(condition)) FAIL_WITH_MESSAGE(errorMessage); } while (0)

namespace JSC {

WASMModuleParser::WASMModuleParser(const SourceCode& source)
    : m_reader(static_cast<WebAssemblySourceProvider*>(source.provider())->data())
{
}

JSWASMModule* WASMModuleParser::parse(VM& vm, JSGlobalObject* globalObject, String& errorMessage)
{
    JSWASMModule* module = JSWASMModule::create(vm, globalObject->wasmModuleStructure());
    parseModule();
    if (!m_errorMessage.isNull()) {
        errorMessage = m_errorMessage;
        return nullptr;
    }
    return module;
}

bool WASMModuleParser::parseModule()
{
    uint32_t magicNumber;
    READ_UNSIGNED_INT32_OR_FAIL(magicNumber, "Cannot read the magic number.");
    FAIL_IF_FALSE(magicNumber == wasmMagicNumber, "The magic number is incorrect.");

    // TODO: parse the rest

    return true;
}

JSWASMModule* parseWebAssembly(ExecState* exec, const SourceCode& source, String& errorMessage)
{
    WASMModuleParser WASMModuleParser(source);
    return WASMModuleParser.parse(exec->vm(), exec->lexicalGlobalObject(), errorMessage);
}

} // namespace JSC

#endif // ENABLE(WEBASSEMBLY)
