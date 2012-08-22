/*
 * Copyright (C) 2012 Apple Inc. All rights reserved.
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
#include "VMInspector.h"

#if ENABLE(VMINSPECTOR)

namespace JSC {

const char* VMInspector::getTypeName(JSValue value)
{
    if (value.isInt32())
        return "<Int32>";
    if (value.isBoolean())
        return "<Boolean>";
    if (value.isNull())
        return "<Empty>";
    if (value.isUndefined())
        return "<Undefined>";
    if (value.isCell())
        return "<Cell>";
    if (value.isEmpty())
        return "<Empty>";
    return "";
}

void VMInspector::dumpFrame0(CallFrame* frame)
{
    dumpFrame(frame, 0, 0, 0, 0);
}

void VMInspector::dumpFrame(CallFrame* frame, const char* prefix,
                            const char* funcName, const char* file, int line)
{
    int frameCount = VMInspector::countFrames(frame);
    if (frameCount < 0)
        return;

    Instruction* vPC = 0;
    if (frame->codeBlock())
        vPC = frame->currentVPC();

    #define CAST reinterpret_cast

    if (prefix)
        printf("%s ", prefix);

    printf("frame [%d] %p { cb %p:%s, retPC %p:%s, scope %p:%s, callee %p:%s, callerFrame %p:%s, argc %d, vPC %p }",
           frameCount, frame,

           CAST<void*>(frame[RegisterFile::CodeBlock].payload()),
           getTypeName(frame[RegisterFile::CodeBlock].jsValue()),

           CAST<void*>(frame[RegisterFile::ReturnPC].payload()),
           getTypeName(frame[RegisterFile::ReturnPC].jsValue()),

           CAST<void*>(frame[RegisterFile::ScopeChain].payload()),
           getTypeName(frame[RegisterFile::ScopeChain].jsValue()),

           CAST<void*>(frame[RegisterFile::Callee].payload()),
           getTypeName(frame[RegisterFile::Callee].jsValue()),

           CAST<void*>(frame[RegisterFile::CallerFrame].payload()),
           getTypeName(frame[RegisterFile::CallerFrame].jsValue()),

           frame[RegisterFile::ArgumentCount].payload(),
           vPC);

    if (funcName || file || (line >= 0)) {
        printf(" @");
        if (funcName)
            printf(" %s", funcName);
        if (file)
            printf(" %s", file);
        if (line >= 0)
            printf(":%d", line);
    }
    printf("\n");
}

int VMInspector::countFrames(CallFrame* frame)
{
    int count = -1;
    while (frame && !frame->hasHostCallFrameFlag()) {
        count++;
        frame = frame->callerFrame();
    }
    return count;
}

} // namespace JSC

#endif // ENABLE(VMINSPECTOR)
