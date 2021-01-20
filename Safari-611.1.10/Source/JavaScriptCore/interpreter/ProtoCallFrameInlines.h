/*
 * Copyright (C) 2019 Apple Inc. All rights reserved.
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

#include "ProtoCallFrame.h"
#include "RegisterInlines.h"

namespace JSC {

inline void ProtoCallFrame::init(CodeBlock* codeBlock, JSGlobalObject* globalObject, JSObject* callee, JSValue thisValue, int argCountIncludingThis, JSValue* otherArgs)
{
    this->args = otherArgs;
    this->setCodeBlock(codeBlock);
    this->setCallee(callee);
    this->setGlobalObject(globalObject);
    this->setArgumentCountIncludingThis(argCountIncludingThis);
    if (codeBlock && argCountIncludingThis < codeBlock->numParameters())
        this->hasArityMismatch = true;
    else
        this->hasArityMismatch = false;

    // Round up argCountIncludingThis to keep the stack frame size aligned.
    size_t paddedArgsCount = roundArgumentCountToAlignFrame(argCountIncludingThis);
    this->setPaddedArgCount(paddedArgsCount);
    this->clearCurrentVPC();
    this->setThisValue(thisValue);
}

inline JSObject* ProtoCallFrame::callee() const
{
    return calleeValue.Register::object();
}

inline void ProtoCallFrame::setCallee(JSObject* callee)
{
    calleeValue = callee;
}

inline CodeBlock* ProtoCallFrame::codeBlock() const
{
    return codeBlockValue.Register::codeBlock();
}

inline void ProtoCallFrame::setCodeBlock(CodeBlock* codeBlock)
{
    codeBlockValue = codeBlock;
}

} // namespace JSC
