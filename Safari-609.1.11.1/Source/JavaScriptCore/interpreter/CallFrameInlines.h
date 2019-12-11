/*
 * Copyright (C) 2018-2019 Apple Inc. All rights reserved.
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

#include "CallFrame.h"
#include "JSCallee.h"
#include "JSGlobalObject.h"
#include "RegisterInlines.h"

namespace JSC {

inline Register& CallFrame::r(int index)
{
    CodeBlock* codeBlock = this->codeBlock();
    if (codeBlock->isConstantRegisterIndex(index))
        return *reinterpret_cast<Register*>(&codeBlock->constantRegister(index));
    return this[index];
}

inline Register& CallFrame::r(VirtualRegister reg)
{
    return r(reg.offset());
}

inline Register& CallFrame::uncheckedR(int index)
{
    RELEASE_ASSERT(index < FirstConstantRegisterIndex);
    return this[index];
}

inline Register& CallFrame::uncheckedR(VirtualRegister reg)
{
    return uncheckedR(reg.offset());
}

inline JSValue CallFrame::guaranteedJSValueCallee() const
{
    ASSERT(!callee().isWasm());
    return this[CallFrameSlot::callee].jsValue();
}

inline JSObject* CallFrame::jsCallee() const
{
    ASSERT(!callee().isWasm());
    return this[CallFrameSlot::callee].object();
}

inline CodeBlock* CallFrame::codeBlock() const
{
    return this[CallFrameSlot::codeBlock].Register::codeBlock();
}

inline SUPPRESS_ASAN CodeBlock* CallFrame::unsafeCodeBlock() const
{
    return this[CallFrameSlot::codeBlock].Register::asanUnsafeCodeBlock();
}

inline JSGlobalObject* CallFrame::lexicalGlobalObject(VM& vm) const
{
    UNUSED_PARAM(vm);
#if ENABLE(WEBASSEMBLY)
    if (callee().isWasm())
        return lexicalGlobalObjectFromWasmCallee(vm);
#endif
    return jsCallee()->globalObject();
}

inline bool CallFrame::isStackOverflowFrame() const
{
    if (callee().isWasm())
        return false;
    return jsCallee() == jsCallee()->globalObject()->stackOverflowFrameCallee();
}

inline bool CallFrame::isWasmFrame() const
{
    return callee().isWasm();
}

inline void CallFrame::setCallee(JSObject* callee)
{
    static_cast<Register*>(this)[CallFrameSlot::callee] = callee;
}

inline void CallFrame::setCodeBlock(CodeBlock* codeBlock)
{
    static_cast<Register*>(this)[CallFrameSlot::codeBlock] = codeBlock;
}

inline void CallFrame::setScope(int scopeRegisterOffset, JSScope* scope)
{
    static_cast<Register*>(this)[scopeRegisterOffset] = scope;
}

inline JSScope* CallFrame::scope(int scopeRegisterOffset) const
{
    ASSERT(this[scopeRegisterOffset].Register::scope());
    return this[scopeRegisterOffset].Register::scope();
}

inline Register* CallFrame::topOfFrame()
{
    if (!codeBlock())
        return registers();
    return topOfFrameInternal();
}

} // namespace JSC
