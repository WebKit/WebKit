/*
 * Copyright (C) 2013-2019 Apple Inc. All rights reserved.
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
#include "TestRunnerUtils.h"

#include "CodeBlock.h"
#include "FunctionCodeBlock.h"
#include "JSCInlines.h"
#include "LLIntData.h"

namespace JSC {

FunctionExecutable* getExecutableForFunction(JSValue theFunctionValue)
{
    if (!theFunctionValue.isCell())
        return nullptr;

    VM& vm = theFunctionValue.asCell()->vm();
    JSFunction* theFunction = jsDynamicCast<JSFunction*>(vm, theFunctionValue);
    if (!theFunction)
        return nullptr;
    
    FunctionExecutable* executable = jsDynamicCast<FunctionExecutable*>(vm, 
        theFunction->executable());
    return executable;
}

CodeBlock* getSomeBaselineCodeBlockForFunction(JSValue theFunctionValue)
{
    FunctionExecutable* executable = getExecutableForFunction(theFunctionValue);
    if (!executable)
        return 0;
    
    CodeBlock* baselineCodeBlock = executable->baselineCodeBlockFor(CodeForCall);
    
    if (!baselineCodeBlock)
        baselineCodeBlock = executable->baselineCodeBlockFor(CodeForConstruct);
    
    return baselineCodeBlock;
}

JSValue numberOfDFGCompiles(JSValue theFunctionValue)
{
    bool pretendToHaveManyCompiles = false;
#if ENABLE(DFG_JIT)
    if (!Options::useJIT() || !Options::useDFGJIT())
        pretendToHaveManyCompiles = true;
#else
    pretendToHaveManyCompiles = true;
#endif

    if (CodeBlock* baselineCodeBlock = getSomeBaselineCodeBlockForFunction(theFunctionValue)) {
        if (pretendToHaveManyCompiles)
            return jsNumber(1000000.0);
        return jsNumber(baselineCodeBlock->numberOfDFGCompiles());
    }
    
    return jsNumber(0);
}

JSValue setNeverInline(JSValue theFunctionValue)
{
    if (FunctionExecutable* executable = getExecutableForFunction(theFunctionValue))
        executable->setNeverInline(true);
    
    return jsUndefined();
}

JSValue setNeverOptimize(JSValue theFunctionValue)
{
    if (FunctionExecutable* executable = getExecutableForFunction(theFunctionValue))
        executable->setNeverOptimize(true);
    
    return jsUndefined();
}

JSValue optimizeNextInvocation(JSValue theFunctionValue)
{
#if ENABLE(JIT)
    if (CodeBlock* baselineCodeBlock = getSomeBaselineCodeBlockForFunction(theFunctionValue))
        baselineCodeBlock->optimizeNextInvocation();
#endif
    UNUSED_PARAM(theFunctionValue);
    return jsUndefined();
}

JSValue failNextNewCodeBlock(JSGlobalObject* globalObject)
{
    globalObject->vm().setFailNextNewCodeBlock();

    return jsUndefined();
}

JSValue numberOfDFGCompiles(JSGlobalObject*, CallFrame* callFrame)
{
    if (callFrame->argumentCount() < 1)
        return jsUndefined();
    return numberOfDFGCompiles(callFrame->uncheckedArgument(0));
}

JSValue setNeverInline(JSGlobalObject*, CallFrame* callFrame)
{
    if (callFrame->argumentCount() < 1)
        return jsUndefined();
    return setNeverInline(callFrame->uncheckedArgument(0));
}

JSValue setNeverOptimize(JSGlobalObject*, CallFrame* callFrame)
{
    if (callFrame->argumentCount() < 1)
        return jsUndefined();
    return setNeverOptimize(callFrame->uncheckedArgument(0));
}

JSValue setCannotUseOSRExitFuzzing(JSGlobalObject*, CallFrame* callFrame)
{
    if (callFrame->argumentCount() < 1)
        return jsUndefined();

    JSValue theFunctionValue = callFrame->uncheckedArgument(0);
    if (FunctionExecutable* executable = getExecutableForFunction(theFunctionValue))
        executable->setCanUseOSRExitFuzzing(false);

    return jsUndefined();
}

JSValue optimizeNextInvocation(JSGlobalObject*, CallFrame* callFrame)
{
    if (callFrame->argumentCount() < 1)
        return jsUndefined();
    return optimizeNextInvocation(callFrame->uncheckedArgument(0));
}

// This is a hook called at the bitter end of some of our tests.
void finalizeStatsAtEndOfTesting()
{
}

} // namespace JSC

