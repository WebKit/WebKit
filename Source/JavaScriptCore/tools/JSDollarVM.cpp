/*
 * Copyright (C) 2015-2017 Apple Inc. All rights reserved.
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
#include "JSDollarVM.h"

#include "CodeBlock.h"
#include "FunctionCodeBlock.h"
#include "JSCInlines.h"
#include "VMInspector.h"
#include <wtf/DataLog.h>
#include <wtf/ProcessID.h>
#include <wtf/StringPrintStream.h>

namespace JSC {

const ClassInfo JSDollarVM::s_info = { "DollarVM", &Base::s_info, nullptr, nullptr, CREATE_METHOD_TABLE(JSDollarVM) };

void JSDollarVM::addFunction(VM& vm, JSGlobalObject* globalObject, const char* name, NativeFunction function, unsigned arguments)
{
    Identifier identifier = Identifier::fromString(&vm, name);
    putDirect(vm, identifier, JSFunction::create(vm, globalObject, arguments, identifier.string(), function));
}

// Triggers a crash immediately.
// Usage: $vm.crash()
static EncodedJSValue JSC_HOST_CALL functionCrash(ExecState*)
{
    CRASH();
    return JSValue::encode(jsUndefined());
}

// Returns true if the current frame is a DFG frame.
// Usage: isDFG = $vm.dfgTrue()
static EncodedJSValue JSC_HOST_CALL functionDFGTrue(ExecState*)
{
    return JSValue::encode(jsBoolean(false));
}

class CallerFrameJITTypeFunctor {
public:
    CallerFrameJITTypeFunctor()
        : m_currentFrame(0)
        , m_jitType(JITCode::None)
    {
    }

    StackVisitor::Status operator()(StackVisitor& visitor) const
    {
        if (m_currentFrame++ > 1) {
            m_jitType = visitor->codeBlock()->jitType();
            return StackVisitor::Done;
        }
        return StackVisitor::Continue;
    }
    
    JITCode::JITType jitType() { return m_jitType; }

private:
    mutable unsigned m_currentFrame;
    mutable JITCode::JITType m_jitType;
};

// Returns true if the current frame is a LLInt frame.
// Usage: isLLInt = $vm.llintTrue()
static EncodedJSValue JSC_HOST_CALL functionLLintTrue(ExecState* exec)
{
    if (!exec)
        return JSValue::encode(jsUndefined());
    CallerFrameJITTypeFunctor functor;
    exec->iterate(functor);
    return JSValue::encode(jsBoolean(functor.jitType() == JITCode::InterpreterThunk));
}

// Returns true if the current frame is a baseline JIT frame.
// Usage: isBaselineJIT = $vm.jitTrue()
static EncodedJSValue JSC_HOST_CALL functionJITTrue(ExecState* exec)
{
    if (!exec)
        return JSValue::encode(jsUndefined());
    CallerFrameJITTypeFunctor functor;
    exec->iterate(functor);
    return JSValue::encode(jsBoolean(functor.jitType() == JITCode::BaselineJIT));
}

// Runs a full GC synchronously.
// Usage: $vm.gc()
static EncodedJSValue JSC_HOST_CALL functionGC(ExecState* exec)
{
    VMInspector::gc(exec);
    return JSValue::encode(jsUndefined());
}

// Runs the edenGC synchronously.
// Usage: $vm.edenGC()
static EncodedJSValue JSC_HOST_CALL functionEdenGC(ExecState* exec)
{
    VMInspector::edenGC(exec);
    return JSValue::encode(jsUndefined());
}

// Gets a token for the CodeBlock for a specified frame index.
// Usage: codeBlockToken = $vm.codeBlockForFrame(0) // frame 0 is the top frame.
static EncodedJSValue JSC_HOST_CALL functionCodeBlockForFrame(ExecState* exec)
{
    if (exec->argumentCount() < 1)
        return JSValue::encode(jsUndefined());

    JSValue value = exec->uncheckedArgument(0);
    if (!value.isUInt32())
        return JSValue::encode(jsUndefined());

    // We need to inc the frame number because the caller would consider
    // its own frame as frame 0. Hence, we need discount the frame for this
    // function.
    unsigned frameNumber = value.asUInt32() + 1;
    CodeBlock* codeBlock = VMInspector::codeBlockForFrame(exec, frameNumber);
    // Though CodeBlock is a JSCell, it is not safe to return it directly back to JS code
    // as it is an internal type that the JS code cannot handle. Hence, we first encode the
    // CodeBlock* as a double token (which is safe for JS code to handle) before returning it.
    return JSValue::encode(JSValue(bitwise_cast<double>(static_cast<uint64_t>(reinterpret_cast<uintptr_t>(codeBlock)))));
}

static CodeBlock* codeBlockFromArg(ExecState* exec)
{
    VM& vm = exec->vm();
    if (exec->argumentCount() < 1)
        return nullptr;

    JSValue value = exec->uncheckedArgument(0);
    CodeBlock* candidateCodeBlock = nullptr;
    if (value.isCell()) {
        JSFunction* func = jsDynamicCast<JSFunction*>(vm, value.asCell());
        if (func) {
            if (func->isHostFunction())
                candidateCodeBlock = nullptr;
            else
                candidateCodeBlock = func->jsExecutable()->eitherCodeBlock();
        }
    } else if (value.isDouble()) {
        // If the value is a double, it may be an encoded CodeBlock* that came from
        // $vm.codeBlockForFrame(). We'll treat it as a candidate codeBlock and check if it's
        // valid below before using.
        candidateCodeBlock = reinterpret_cast<CodeBlock*>(bitwise_cast<uint64_t>(value.asDouble()));
    }

    if (candidateCodeBlock && VMInspector::isValidCodeBlock(exec, candidateCodeBlock))
        return candidateCodeBlock;

    if (candidateCodeBlock)
        dataLog("Invalid codeBlock: ", RawPointer(candidateCodeBlock), " ", value, "\n");
    else
        dataLog("Invalid codeBlock: ", value, "\n");
    return nullptr;
}

// Usage: print("codeblock = " + $vm.codeBlockFor(functionObj))
// Usage: print("codeblock = " + $vm.codeBlockFor(codeBlockToken))
static EncodedJSValue JSC_HOST_CALL functionCodeBlockFor(ExecState* exec)
{
    CodeBlock* codeBlock = codeBlockFromArg(exec);
    WTF::StringPrintStream stream;
    if (codeBlock) {
        stream.print(*codeBlock);
        return JSValue::encode(jsString(exec, stream.toString()));
    }
    return JSValue::encode(jsUndefined());
}

// Usage: $vm.printSourceFor(functionObj)
// Usage: $vm.printSourceFor(codeBlockToken)
static EncodedJSValue JSC_HOST_CALL functionPrintSourceFor(ExecState* exec)
{
    CodeBlock* codeBlock = codeBlockFromArg(exec);
    if (codeBlock)
        codeBlock->dumpSource();
    return JSValue::encode(jsUndefined());
}

// Usage: $vm.printBytecodeFor(functionObj)
// Usage: $vm.printBytecode(codeBlockToken)
static EncodedJSValue JSC_HOST_CALL functionPrintBytecodeFor(ExecState* exec)
{
    CodeBlock* codeBlock = codeBlockFromArg(exec);
    if (codeBlock)
        codeBlock->dumpBytecode();
    return JSValue::encode(jsUndefined());
}

// Prints a series of comma separate strings without inserting a newline.
// Usage: $vm.print(str1, str2, str3)
static EncodedJSValue JSC_HOST_CALL functionPrint(ExecState* exec)
{
    auto scope = DECLARE_THROW_SCOPE(exec->vm());
    for (unsigned i = 0; i < exec->argumentCount(); ++i) {
        String argStr = exec->uncheckedArgument(i).toWTFString(exec);
        RETURN_IF_EXCEPTION(scope, encodedJSValue());
        dataLog(argStr);
    }
    return JSValue::encode(jsUndefined());
}

// Prints the current CallFrame.
// Usage: $vm.printCallFrame()
static EncodedJSValue JSC_HOST_CALL functionPrintCallFrame(ExecState* exec)
{
    // When the callers call this function, they are expecting to print their
    // own frame. So skip 1 for this frame.
    VMInspector::printCallFrame(exec, 1);
    return JSValue::encode(jsUndefined());
}

// Prints the JS stack.
// Usage: $vm.printStack()
static EncodedJSValue JSC_HOST_CALL functionPrintStack(ExecState* exec)
{
    // When the callers call this function, they are expecting to print the
    // stack starting their own frame. So skip 1 for this frame.
    VMInspector::printStack(exec, 1);
    return JSValue::encode(jsUndefined());
}

// Gets the dataLog dump of a given JS value as a string.
// Usage: print("value = " + $vm.value(jsValue))
static EncodedJSValue JSC_HOST_CALL functionValue(ExecState* exec)
{
    WTF::StringPrintStream stream;
    for (unsigned i = 0; i < exec->argumentCount(); ++i) {
        if (i)
            stream.print(", ");
        stream.print(exec->uncheckedArgument(i));
    }
    
    return JSValue::encode(jsString(exec, stream.toString()));
}

// Gets the pid of the current process.
// Usage: print("pid = " + $vm.getpid())
static EncodedJSValue JSC_HOST_CALL functionGetPID(ExecState*)
{
    return JSValue::encode(jsNumber(getCurrentProcessID()));
}

void JSDollarVM::finishCreation(VM& vm, JSGlobalObject* globalObject)
{
    Base::finishCreation(vm);
    
    addFunction(vm, globalObject, "crash", functionCrash, 0);
    
    putDirectNativeFunction(vm, globalObject, Identifier::fromString(&vm, "dfgTrue"), 0, functionDFGTrue, DFGTrueIntrinsic, static_cast<unsigned>(PropertyAttribute::DontEnum));
    
    addFunction(vm, globalObject, "llintTrue", functionLLintTrue, 0);
    addFunction(vm, globalObject, "jitTrue", functionJITTrue, 0);
    
    addFunction(vm, globalObject, "gc", functionGC, 0);
    addFunction(vm, globalObject, "edenGC", functionEdenGC, 0);
    
    addFunction(vm, globalObject, "codeBlockFor", functionCodeBlockFor, 1);
    addFunction(vm, globalObject, "codeBlockForFrame", functionCodeBlockForFrame, 1);
    addFunction(vm, globalObject, "printSourceFor", functionPrintSourceFor, 1);
    addFunction(vm, globalObject, "printBytecodeFor", functionPrintBytecodeFor, 1);
    
    addFunction(vm, globalObject, "print", functionPrint, 1);
    addFunction(vm, globalObject, "printCallFrame", functionPrintCallFrame, 0);
    addFunction(vm, globalObject, "printStack", functionPrintStack, 0);

    addFunction(vm, globalObject, "value", functionValue, 1);
    addFunction(vm, globalObject, "getpid", functionGetPID, 0);
}

} // namespace JSC
