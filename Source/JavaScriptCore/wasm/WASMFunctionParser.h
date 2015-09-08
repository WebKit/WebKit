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

#ifndef WASMFunctionParser_h
#define WASMFunctionParser_h

#if ENABLE(WEBASSEMBLY)

#include "SourceCode.h"
#include "WASMReader.h"

#define ContextExpression typename Context::Expression
#define ContextStatement typename Context::Statement

namespace JSC {

class CodeBlock;
class JSWASMModule;
class VM;

class WASMFunctionParser {
public:
    static bool checkSyntax(JSWASMModule*, const SourceCode&, size_t functionIndex, unsigned startOffsetInSource, unsigned& endOffsetInSource, unsigned& stackHeight, String& errorMessage);
    static void compile(VM&, CodeBlock*, JSWASMModule*, const SourceCode&, size_t functionIndex);

private:
    WASMFunctionParser(JSWASMModule* module, const SourceCode& source, size_t functionIndex)
        : m_module(module)
        , m_reader(static_cast<WebAssemblySourceProvider*>(source.provider())->data())
        , m_functionIndex(functionIndex)
        , m_breakScopeDepth(0)
        , m_continueScopeDepth(0)
        , m_labelDepth(0)
    {
    }

    template <class Context> bool parseFunction(Context&);
    bool parseLocalVariables();

    template <class Context> ContextStatement parseStatement(Context&);
    template <class Context> ContextStatement parseSetLocalStatement(Context&, uint32_t localIndex);
    template <class Context> ContextStatement parseSetLocalStatement(Context&);
    template <class Context> ContextStatement parseReturnStatement(Context&);
    template <class Context> ContextStatement parseBlockStatement(Context&);
    template <class Context> ContextStatement parseIfStatement(Context&);
    template <class Context> ContextStatement parseIfElseStatement(Context&);
    template <class Context> ContextStatement parseWhileStatement(Context&);
    template <class Context> ContextStatement parseDoStatement(Context&);
    template <class Context> ContextStatement parseLabelStatement(Context&);
    template <class Context> ContextStatement parseBreakStatement(Context&);
    template <class Context> ContextStatement parseBreakLabelStatement(Context&);
    template <class Context> ContextStatement parseContinueStatement(Context&);
    template <class Context> ContextStatement parseContinueLabelStatement(Context&);
    template <class Context> ContextStatement parseSwitchStatement(Context&);

    template <class Context> ContextExpression parseExpression(Context&, WASMExpressionType);

    template <class Context> ContextExpression parseExpressionI32(Context&);
    template <class Context> ContextExpression parseConstantPoolIndexExpressionI32(Context&, uint32_t constantPoolIndex);
    template <class Context> ContextExpression parseConstantPoolIndexExpressionI32(Context&);
    template <class Context> ContextExpression parseImmediateExpressionI32(Context&, uint32_t immediate);
    template <class Context> ContextExpression parseImmediateExpressionI32(Context&);
    template <class Context> ContextExpression parseGetLocalExpressionI32(Context&, uint32_t localIndex);
    template <class Context> ContextExpression parseGetLocalExpressionI32(Context&);
    template <class Context> ContextExpression parseBinaryExpressionI32(Context&, WASMOpExpressionI32);
    template <class Context> ContextExpression parseRelationalI32ExpressionI32(Context&, WASMOpExpressionI32);

    template <class Context> ContextExpression parseExpressionF64(Context&);
    template <class Context> ContextExpression parseConstantPoolIndexExpressionF64(Context&, uint32_t constantIndex);
    template <class Context> ContextExpression parseConstantPoolIndexExpressionF64(Context&);
    template <class Context> ContextExpression parseImmediateExpressionF64(Context&);
    template <class Context> ContextExpression parseGetLocalExpressionF64(Context&, uint32_t localIndex);
    template <class Context> ContextExpression parseGetLocalExpressionF64(Context&);

    JSWASMModule* m_module;
    WASMReader m_reader;
    size_t m_functionIndex;
    String m_errorMessage;

    WASMExpressionType m_returnType;
    Vector<WASMType> m_localTypes;
    uint32_t m_numberOfI32LocalVariables;
    uint32_t m_numberOfF32LocalVariables;
    uint32_t m_numberOfF64LocalVariables;

    unsigned m_breakScopeDepth;
    unsigned m_continueScopeDepth;
    unsigned m_labelDepth;
};

} // namespace JSC

#endif // ENABLE(WEBASSEMBLY)

#endif // WASMFunctionParser_h
