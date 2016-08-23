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
#include "StackFrame.h"

#include "CodeBlock.h"
#include "DebuggerPrimitives.h"
#include "JSCInlines.h"
#include <wtf/text/StringBuilder.h>

namespace JSC {

intptr_t StackFrame::sourceID() const
{
    if (!codeBlock)
        return noSourceID;
    return codeBlock->ownerScriptExecutable()->sourceID();
}

String StackFrame::sourceURL() const
{
    if (!codeBlock)
        return ASCIILiteral("[native code]");

    String sourceURL = codeBlock->ownerScriptExecutable()->sourceURL();
    if (!sourceURL.isNull())
        return sourceURL;
    return emptyString();
}

String StackFrame::functionName(VM& vm) const
{
    if (codeBlock) {
        switch (codeBlock->codeType()) {
        case EvalCode:
            return ASCIILiteral("eval code");
        case ModuleCode:
            return ASCIILiteral("module code");
        case FunctionCode:
            break;
        case GlobalCode:
            return ASCIILiteral("global code");
        default:
            ASSERT_NOT_REACHED();
        }
    }
    String name;
    if (callee)
        name = getCalculatedDisplayName(vm, callee.get()).impl();
    return name.isNull() ? emptyString() : name;
}

void StackFrame::computeLineAndColumn(unsigned& line, unsigned& column) const
{
    if (!codeBlock) {
        line = 0;
        column = 0;
        return;
    }

    int divot = 0;
    int unusedStartOffset = 0;
    int unusedEndOffset = 0;
    codeBlock->expressionRangeForBytecodeOffset(bytecodeOffset, divot, unusedStartOffset, unusedEndOffset, line, column);

    ScriptExecutable* executable = codeBlock->ownerScriptExecutable();
    if (executable->hasOverrideLineNumber())
        line = executable->overrideLineNumber();
}

String StackFrame::toString(VM& vm) const
{
    StringBuilder traceBuild;
    String functionName = this->functionName(vm);
    String sourceURL = this->sourceURL();
    traceBuild.append(functionName);
    if (!sourceURL.isEmpty()) {
        if (!functionName.isEmpty())
            traceBuild.append('@');
        traceBuild.append(sourceURL);
        if (codeBlock) {
            unsigned line;
            unsigned column;
            computeLineAndColumn(line, column);

            traceBuild.append(':');
            traceBuild.appendNumber(line);
            traceBuild.append(':');
            traceBuild.appendNumber(column);
        }
    }
    return traceBuild.toString().impl();
}

} // namespace JSC

