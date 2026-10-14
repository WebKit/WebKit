/*
 * Copyright (C) 2015-2019 Apple Inc. All rights reserved.
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
#include "testb3.h"

#include <wtf/Int128.h>

WTF_ALLOW_UNSAFE_BUFFER_USAGE_BEGIN

#if ENABLE(B3_JIT)

template<typename T>
void testAtomicWeakCAS()
{
    constexpr Type type = NativeTraits<T>::type;
    constexpr Width width = NativeTraits<T>::width;

    auto checkMyDisassembly = [&] (Compilation& compilation, bool fenced) {
        if (isX86()) {
            checkUsesInstruction(compilation, "lock");
            checkUsesInstruction(compilation, "cmpxchg");
        } else {
            if (isARM64_LSE())
                checkUsesInstruction(compilation, "casal");
            else {
                if (fenced) {
                    checkUsesInstruction(compilation, "ldax");
                    checkUsesInstruction(compilation, "stlx");
                } else {
                    checkUsesInstruction(compilation, "ldx");
                    checkUsesInstruction(compilation, "stx");
                }
            }
        }
    };

    {
        Procedure proc;
        BasicBlock* root = proc.addBlock();
        BasicBlock* reloop = proc.addBlock();
        BasicBlock* done = proc.addBlock();
        auto arguments = cCallArgumentValues<void*>(proc, root);
    
        Value* ptr = arguments[0];
        root->appendNew<Value>(proc, Jump, Origin());
        root->setSuccessors(reloop);
    
        reloop->appendNew<Value>(
            proc, Branch, Origin(),
            reloop->appendNew<AtomicValue>(
                proc, AtomicWeakCAS, Origin(), width,
                reloop->appendIntConstant(proc, Origin(), type, 42),
                reloop->appendIntConstant(proc, Origin(), type, 0xbeef),
                ptr));
        reloop->setSuccessors(done, reloop);
    
        done->appendNew<Value>(proc, Return, Origin());
    
        auto code = compileProc(proc);
        T value[2];
        value[0] = 42;
        value[1] = 13;
        invoke<void>(*code, value);
        CHECK_EQ(value[0], static_cast<T>(0xbeef));
        CHECK_EQ(value[1], 13);
        checkMyDisassembly(*code, true);
    }

    {
        Procedure proc;
        BasicBlock* root = proc.addBlock();
        BasicBlock* reloop = proc.addBlock();
        BasicBlock* done = proc.addBlock();
        auto arguments = cCallArgumentValues<void*>(proc, root);
    
        Value* ptr = arguments[0];
        root->appendNew<Value>(proc, Jump, Origin());
        root->setSuccessors(reloop);
    
        reloop->appendNew<Value>(
            proc, Branch, Origin(),
            reloop->appendNew<AtomicValue>(
                proc, AtomicWeakCAS, Origin(), width,
                reloop->appendIntConstant(proc, Origin(), type, 42),
                reloop->appendIntConstant(proc, Origin(), type, 0xbeef),
                ptr, 0, HeapRange(42), HeapRange()));
        reloop->setSuccessors(done, reloop);
    
        done->appendNew<Value>(proc, Return, Origin());
    
        auto code = compileProc(proc);
        T value[2];
        value[0] = 42;
        value[1] = 13;
        invoke<void>(*code, value);
        CHECK_EQ(value[0], static_cast<T>(0xbeef));
        CHECK_EQ(value[1], 13);
        checkMyDisassembly(*code, false);
    }

    {
        Procedure proc;
        BasicBlock* root = proc.addBlock();
        BasicBlock* succ = proc.addBlock();
        BasicBlock* fail = proc.addBlock();
        auto arguments = cCallArgumentValues<void*>(proc, root);
    
        Value* ptr = arguments[0];
        root->appendNew<Value>(
            proc, Branch, Origin(),
            root->appendNew<AtomicValue>(
                proc, AtomicWeakCAS, Origin(), width,
                root->appendIntConstant(proc, Origin(), type, 42),
                root->appendIntConstant(proc, Origin(), type, 0xbeef),
                ptr));
        root->setSuccessors(succ, fail);
    
        succ->appendNew<MemoryValue>(
            proc, storeOpcode(GP, width), Origin(),
            succ->appendIntConstant(proc, Origin(), type, 100),
            ptr);
        succ->appendNew<Value>(proc, Return, Origin());
    
        fail->appendNew<Value>(proc, Return, Origin());
    
        auto code = compileProc(proc);
        T value[2];
        value[0] = 42;
        value[1] = 13;
        while (value[0] == 42)
            invoke<void>(*code, value);
        CHECK_EQ(value[0], static_cast<T>(100));
        CHECK_EQ(value[1], 13);
        value[0] = static_cast<T>(300);
        invoke<void>(*code, value);
        CHECK_EQ(value[0], static_cast<T>(300));
        CHECK_EQ(value[1], 13);
        checkMyDisassembly(*code, true);
    }

    {
        Procedure proc;
        BasicBlock* root = proc.addBlock();
        BasicBlock* succ = proc.addBlock();
        BasicBlock* fail = proc.addBlock();
        auto arguments = cCallArgumentValues<void*>(proc, root);
    
        Value* ptr = arguments[0];
        root->appendNew<Value>(
            proc, Branch, Origin(),
            root->appendNew<Value>(
                proc, Equal, Origin(),
                root->appendNew<AtomicValue>(
                    proc, AtomicWeakCAS, Origin(), width,
                    root->appendIntConstant(proc, Origin(), type, 42),
                    root->appendIntConstant(proc, Origin(), type, 0xbeef),
                    ptr),
                root->appendIntConstant(proc, Origin(), Int32, 0)));
        root->setSuccessors(fail, succ);
    
        succ->appendNew<MemoryValue>(
            proc, storeOpcode(GP, width), Origin(),
            succ->appendIntConstant(proc, Origin(), type, 100),
            ptr);
        succ->appendNew<Value>(proc, Return, Origin());
    
        fail->appendNew<Value>(proc, Return, Origin());
    
        auto code = compileProc(proc);
        T value[2];
        value[0] = 42;
        value[1] = 13;
        while (value[0] == 42)
            invoke<void>(*code, value);
        CHECK_EQ(value[0], static_cast<T>(100));
        CHECK_EQ(value[1], 13);
        value[0] = static_cast<T>(300);
        invoke<void>(*code, value);
        CHECK_EQ(value[0], static_cast<T>(300));
        CHECK_EQ(value[1], 13);
        checkMyDisassembly(*code, true);
    }

    {
        Procedure proc;
        BasicBlock* root = proc.addBlock();
        auto arguments = cCallArgumentValues<void*>(proc, root);
        root->appendNew<Value>(
            proc, Return, Origin(),
            root->appendNew<AtomicValue>(
                proc, AtomicWeakCAS, Origin(), width,
                root->appendIntConstant(proc, Origin(), type, 42),
                root->appendIntConstant(proc, Origin(), type, 0xbeef),
                arguments[0]));
    
        auto code = compileProc(proc);
        T value[2];
        value[0] = 42;
        value[1] = 13;
        while (!invoke<bool>(*code, value)) { }
        CHECK_EQ(value[0], static_cast<T>(0xbeef));
        CHECK_EQ(value[1], 13);
    
        value[0] = static_cast<T>(300);
        CHECK(!invoke<bool>(*code, value));
        CHECK_EQ(value[0], static_cast<T>(300));
        CHECK_EQ(value[1], 13);
        checkMyDisassembly(*code, true);
    }

    {
        Procedure proc;
        BasicBlock* root = proc.addBlock();
        auto arguments = cCallArgumentValues<void*>(proc, root);
        root->appendNew<Value>(
            proc, Return, Origin(),
            root->appendNew<Value>(
                proc, Equal, Origin(),
                root->appendNew<AtomicValue>(
                    proc, AtomicWeakCAS, Origin(), width,
                    root->appendIntConstant(proc, Origin(), type, 42),
                    root->appendIntConstant(proc, Origin(), type, 0xbeef),
                    arguments[0]),
                root->appendNew<Const32Value>(proc, Origin(), 0)));
    
        auto code = compileProc(proc);
        T value[2];
        value[0] = 42;
        value[1] = 13;
        while (invoke<bool>(*code, value)) { }
        CHECK_EQ(value[0], static_cast<T>(0xbeef));
        CHECK_EQ(value[1], 13);
    
        value[0] = static_cast<T>(300);
        CHECK(invoke<bool>(*code, value));
        CHECK_EQ(value[0], static_cast<T>(300));
        CHECK_EQ(value[1], 13);
        checkMyDisassembly(*code, true);
    }

    {
        Procedure proc;
        BasicBlock* root = proc.addBlock();
        auto arguments = cCallArgumentValues<void*>(proc, root);
        root->appendNew<Value>(
            proc, Return, Origin(),
            root->appendNew<AtomicValue>(
                proc, AtomicWeakCAS, Origin(), width,
                root->appendIntConstant(proc, Origin(), type, 42),
                root->appendIntConstant(proc, Origin(), type, 0xbeef),
                arguments[0],
                42));
    
        auto code = compileProc(proc);
        T value[2];
        value[0] = 42;
        value[1] = 13;
        while (!invoke<bool>(*code, reinterpret_cast<intptr_t>(value) - 42)) { }
        CHECK_EQ(value[0], static_cast<T>(0xbeef));
        CHECK_EQ(value[1], 13);
    
        value[0] = static_cast<T>(300);
        CHECK(!invoke<bool>(*code, reinterpret_cast<intptr_t>(value) - 42));
        CHECK_EQ(value[0], static_cast<T>(300));
        CHECK_EQ(value[1], 13);
        checkMyDisassembly(*code, true);
    }
}

template<typename T>
void testAtomicStrongCAS()
{
    constexpr Type type = NativeTraits<T>::type;
    constexpr Width width = NativeTraits<T>::width;

    auto checkMyDisassembly = [&] (Compilation& compilation, bool fenced) {
        if (isX86()) {
            checkUsesInstruction(compilation, "lock");
            checkUsesInstruction(compilation, "cmpxchg");
        } else {
            if (isARM64_LSE())
                checkUsesInstruction(compilation, "casal");
            else {
                if (fenced) {
                    checkUsesInstruction(compilation, "ldax");
                    checkUsesInstruction(compilation, "stlx");
                } else {
                    checkUsesInstruction(compilation, "ldx");
                    checkUsesInstruction(compilation, "stx");
                }
            }
        }
    };

    {
        Procedure proc;
        BasicBlock* root = proc.addBlock();
        BasicBlock* succ = proc.addBlock();
        BasicBlock* fail = proc.addBlock();
        auto arguments = cCallArgumentValues<void*>(proc, root);
    
        Value* ptr = arguments[0];
        root->appendNew<Value>(
            proc, Branch, Origin(),
            root->appendNew<Value>(
                proc, Equal, Origin(),
                root->appendNew<AtomicValue>(
                    proc, AtomicStrongCAS, Origin(), width,
                    root->appendIntConstant(proc, Origin(), type, 42),
                    root->appendIntConstant(proc, Origin(), type, 0xbeef),
                    ptr),
                root->appendIntConstant(proc, Origin(), type, 42)));
        root->setSuccessors(succ, fail);
    
        succ->appendNew<MemoryValue>(
            proc, storeOpcode(GP, width), Origin(),
            succ->appendIntConstant(proc, Origin(), type, 100),
            ptr);
        succ->appendNew<Value>(proc, Return, Origin());
    
        fail->appendNew<Value>(proc, Return, Origin());
    
        auto code = compileProc(proc);
        T value[2];
        value[0] = 42;
        value[1] = 13;
        invoke<void>(*code, value);
        CHECK_EQ(value[0], static_cast<T>(100));
        CHECK_EQ(value[1], 13);
        value[0] = static_cast<T>(300);
        invoke<void>(*code, value);
        CHECK_EQ(value[0], static_cast<T>(300));
        CHECK_EQ(value[1], 13);
        checkMyDisassembly(*code, true);
    }

    {
        Procedure proc;
        BasicBlock* root = proc.addBlock();
        BasicBlock* succ = proc.addBlock();
        BasicBlock* fail = proc.addBlock();
        auto arguments = cCallArgumentValues<void*>(proc, root);
    
        Value* ptr = arguments[0];
        root->appendNew<Value>(
            proc, Branch, Origin(),
            root->appendNew<Value>(
                proc, Equal, Origin(),
                root->appendNew<AtomicValue>(
                    proc, AtomicStrongCAS, Origin(), width,
                    root->appendIntConstant(proc, Origin(), type, 42),
                    root->appendIntConstant(proc, Origin(), type, 0xbeef),
                    ptr, 0, HeapRange(42), HeapRange()),
                root->appendIntConstant(proc, Origin(), type, 42)));
        root->setSuccessors(succ, fail);
    
        succ->appendNew<MemoryValue>(
            proc, storeOpcode(GP, width), Origin(),
            succ->appendIntConstant(proc, Origin(), type, 100),
            ptr);
        succ->appendNew<Value>(proc, Return, Origin());
    
        fail->appendNew<Value>(proc, Return, Origin());
    
        auto code = compileProc(proc);
        T value[2];
        value[0] = 42;
        value[1] = 13;
        invoke<void>(*code, value);
        CHECK_EQ(value[0], static_cast<T>(100));
        CHECK_EQ(value[1], 13);
        value[0] = static_cast<T>(300);
        invoke<void>(*code, value);
        CHECK_EQ(value[0], static_cast<T>(300));
        CHECK_EQ(value[1], 13);
        checkMyDisassembly(*code, false);
    }

    {
        Procedure proc;
        BasicBlock* root = proc.addBlock();
        BasicBlock* succ = proc.addBlock();
        BasicBlock* fail = proc.addBlock();
        auto arguments = cCallArgumentValues<void*>(proc, root);
    
        Value* ptr = arguments[0];
        root->appendNew<Value>(
            proc, Branch, Origin(),
            root->appendNew<Value>(
                proc, NotEqual, Origin(),
                root->appendNew<AtomicValue>(
                    proc, AtomicStrongCAS, Origin(), width,
                    root->appendIntConstant(proc, Origin(), type, 42),
                    root->appendIntConstant(proc, Origin(), type, 0xbeef),
                    ptr),
                root->appendIntConstant(proc, Origin(), type, 42)));
        root->setSuccessors(fail, succ);
    
        succ->appendNew<MemoryValue>(
            proc, storeOpcode(GP, width), Origin(),
            succ->appendIntConstant(proc, Origin(), type, 100),
            ptr);
        succ->appendNew<Value>(proc, Return, Origin());
    
        fail->appendNew<Value>(proc, Return, Origin());
    
        auto code = compileProc(proc);
        T value[2];
        value[0] = 42;
        value[1] = 13;
        invoke<void>(*code, value);
        CHECK_EQ(value[0], static_cast<T>(100));
        CHECK_EQ(value[1], 13);
        value[0] = static_cast<T>(300);
        invoke<void>(*code, value);
        CHECK_EQ(value[0], static_cast<T>(300));
        CHECK_EQ(value[1], 13);
        checkMyDisassembly(*code, true);
    }

    {
        Procedure proc;
        BasicBlock* root = proc.addBlock();
        auto arguments = cCallArgumentValues<void*>(proc, root);
        root->appendNew<Value>(
            proc, Return, Origin(),
            root->appendNew<AtomicValue>(
                proc, AtomicStrongCAS, Origin(), width,
                root->appendIntConstant(proc, Origin(), type, 42),
                root->appendIntConstant(proc, Origin(), type, 0xbeef),
                arguments[0]));
    
        auto code = compileProc(proc);
        T value[2];
        value[0] = 42;
        value[1] = 13;
        CHECK_EQ(invoke<typename NativeTraits<T>::CanonicalType>(*code, value), 42);
        CHECK_EQ(value[0], static_cast<T>(0xbeef));
        CHECK_EQ(value[1], 13);
        value[0] = static_cast<T>(300);
        CHECK_EQ(invoke<typename NativeTraits<T>::CanonicalType>(*code, value), static_cast<typename NativeTraits<T>::CanonicalType>(static_cast<T>(300)));
        CHECK_EQ(value[0], static_cast<T>(300));
        CHECK_EQ(value[1], 13);
        value[0] = static_cast<T>(-1);
        CHECK_EQ(invoke<typename NativeTraits<T>::CanonicalType>(*code, value), static_cast<typename NativeTraits<T>::CanonicalType>(static_cast<T>(-1)));
        CHECK_EQ(value[0], static_cast<T>(-1));
        CHECK_EQ(value[1], 13);
        checkMyDisassembly(*code, true);
    }

    {
        // Test for https://bugs.webkit.org/show_bug.cgi?id=169867.
    
        Procedure proc;
        BasicBlock* root = proc.addBlock();
        auto arguments = cCallArgumentValues<void*>(proc, root);
        root->appendNew<Value>(
            proc, Return, Origin(),
            root->appendNew<Value>(
                proc, BitXor, Origin(),
                root->appendNew<AtomicValue>(
                    proc, AtomicStrongCAS, Origin(), width,
                    root->appendIntConstant(proc, Origin(), type, 42),
                    root->appendIntConstant(proc, Origin(), type, 0xbeef),
                    arguments[0]),
                root->appendIntConstant(proc, Origin(), type, 1)));
    
        typename NativeTraits<T>::CanonicalType one = 1;
    
        auto code = compileProc(proc);
        T value[2];
        value[0] = 42;
        value[1] = 13;
        CHECK_EQ(invoke<typename NativeTraits<T>::CanonicalType>(*code, value), 42 ^ one);
        CHECK_EQ(value[0], static_cast<T>(0xbeef));
        CHECK_EQ(value[1], 13);
        value[0] = static_cast<T>(300);
        CHECK_EQ(invoke<typename NativeTraits<T>::CanonicalType>(*code, value), static_cast<typename NativeTraits<T>::CanonicalType>(static_cast<T>(300)) ^ one);
        CHECK_EQ(value[0], static_cast<T>(300));
        CHECK_EQ(value[1], 13);
        value[0] = static_cast<T>(-1);
        CHECK_EQ(invoke<typename NativeTraits<T>::CanonicalType>(*code, value), static_cast<typename NativeTraits<T>::CanonicalType>(static_cast<T>(-1)) ^ one);
        CHECK_EQ(value[0], static_cast<T>(-1));
        CHECK_EQ(value[1], 13);
        checkMyDisassembly(*code, true);
    }

    {
        Procedure proc;
        BasicBlock* root = proc.addBlock();
        auto arguments = cCallArgumentValues<void*>(proc, root);
        root->appendNew<Value>(
            proc, Return, Origin(),
            root->appendNew<Value>(
                proc, Equal, Origin(),
                root->appendNew<AtomicValue>(
                    proc, AtomicStrongCAS, Origin(), width,
                    root->appendIntConstant(proc, Origin(), type, 42),
                    root->appendIntConstant(proc, Origin(), type, 0xbeef),
                    arguments[0]),
                root->appendIntConstant(proc, Origin(), type, 42)));
    
        auto code = compileProc(proc);
        T value[2];
        value[0] = 42;
        value[1] = 13;
        CHECK(invoke<bool>(*code, value));
        CHECK_EQ(value[0], static_cast<T>(0xbeef));
        CHECK_EQ(value[1], 13);
        value[0] = static_cast<T>(300);
        CHECK(!invoke<bool>(*code, value));
        CHECK_EQ(value[0], static_cast<T>(300));
        CHECK_EQ(value[1], 13);
        checkMyDisassembly(*code, true);
    }

    {
        Procedure proc;
        BasicBlock* root = proc.addBlock();
        auto arguments = cCallArgumentValues<void*>(proc, root);
        root->appendNew<Value>(
            proc, Return, Origin(),
            root->appendNew<Value>(
                proc, Equal, Origin(),
                root->appendNew<Value>(
                    proc, NotEqual, Origin(),
                    root->appendNew<AtomicValue>(
                        proc, AtomicStrongCAS, Origin(), width,
                        root->appendIntConstant(proc, Origin(), type, 42),
                        root->appendIntConstant(proc, Origin(), type, 0xbeef),
                        arguments[0]),
                    root->appendIntConstant(proc, Origin(), type, 42)),
                root->appendNew<Const32Value>(proc, Origin(), 0)));
        
        auto code = compileProc(proc);
        T value[2];
        value[0] = 42;
        value[1] = 13;
        CHECK(invoke<bool>(*code, value));
        CHECK_EQ(value[0], static_cast<T>(0xbeef));
        CHECK_EQ(value[1], 13);
        value[0] = static_cast<T>(300);
        CHECK(!invoke<bool>(*code, &value));
        CHECK_EQ(value[0], static_cast<T>(300));
        CHECK_EQ(value[1], 13);
        checkMyDisassembly(*code, true);
    }

    {
        Procedure proc;
        BasicBlock* root = proc.addBlock();
        auto arguments = cCallArgumentValues<void*>(proc, root);

        Value* ptr = arguments[0];
        root->appendNew<Value>(
            proc, Return, Origin(),
            root->appendNew<AtomicValue>(
                proc, AtomicStrongCAS, Origin(), width,
                root->appendIntConstant(proc, Origin(), type, 0x0f00000000000000ULL + 42),
                root->appendIntConstant(proc, Origin(), type, 0xbeef),
                ptr));

        auto code = compileProc(proc);
        T value[2];
        T result;
        value[0] = 42;
        value[1] = 13;
        result = invoke<T>(*code, value);
        if (width == Width64)
            CHECK_EQ(value[0], static_cast<T>(42));
        else
            CHECK_EQ(value[0], static_cast<T>(0xbeef));
        CHECK_EQ(value[1], 13);
        CHECK_EQ(result, static_cast<T>(42));
        value[0] = static_cast<T>(300);
        result = invoke<T>(*code, value);
        CHECK_EQ(value[0], static_cast<T>(300));
        CHECK_EQ(value[1], 13);
        CHECK_EQ(result, static_cast<T>(300));
        checkMyDisassembly(*code, true);
    }
}

template<typename T>
void testAtomicXchg(B3::Opcode opcode)
{
    constexpr Type type = NativeTraits<T>::type;
    constexpr Width width = NativeTraits<T>::width;

    auto doTheMath = [&] (T& memory, T operand) -> T {
        T oldValue = memory;
        switch (opcode) {
        case AtomicXchgAdd:
            memory += operand;
            break;
        case AtomicXchgAnd:
            memory &= operand;
            break;
        case AtomicXchgOr:
            memory |= operand;
            break;
        case AtomicXchgSub:
            memory -= operand;
            break;
        case AtomicXchgXor:
            memory ^= operand;
            break;
        case AtomicXchg:
            memory = operand;
            break;
        default:
            RELEASE_ASSERT_NOT_REACHED();
        }
        return oldValue;
    };

    auto oldValue = [&] (T memory, T operand) -> T {
        return doTheMath(memory, operand);
    };

    auto newValue = [&] (T memory, T operand) -> T {
        doTheMath(memory, operand);
        return memory;
    };

    auto checkMyDisassembly = [&] (Compilation& compilation, bool fenced) {
        if (isX86()) {
            // AtomicXchg can be lowered to "xchg" without "lock", and this is OK since "lock" signal is asserted for "xchg" by default.
            if (AtomicXchg != opcode)
                checkUsesInstruction(compilation, "lock");
        } else {
            if (isARM64_LSE()) {
                switch (opcode) {
                case AtomicXchgAdd:
                    checkUsesInstruction(compilation, "ldaddal");
                    break;
                case AtomicXchgAnd:
                    checkUsesInstruction(compilation, "ldclral");
                    break;
                case AtomicXchgOr:
                    checkUsesInstruction(compilation, "ldsetal");
                    break;
                case AtomicXchgSub:
                    checkUsesInstruction(compilation, "ldaddal");
                    break;
                case AtomicXchgXor:
                    checkUsesInstruction(compilation, "ldeoral");
                    break;
                case AtomicXchg:
                    checkUsesInstruction(compilation, "swpal");
                    break;
                default:
                    RELEASE_ASSERT_NOT_REACHED();
                }
            } else {
                if (fenced) {
                    checkUsesInstruction(compilation, "ldax");
                    checkUsesInstruction(compilation, "stlx");
                } else {
                    checkUsesInstruction(compilation, "ldx");
                    checkUsesInstruction(compilation, "stx");
                }
            }
        }
    };

    {
        Procedure proc;
        BasicBlock* root = proc.addBlock();
        auto arguments = cCallArgumentValues<void*>(proc, root);
        root->appendNew<Value>(
            proc, Return, Origin(),
            root->appendNew<AtomicValue>(
                proc, opcode, Origin(), width,
                root->appendIntConstant(proc, Origin(), type, 1),
                arguments[0]));

        auto code = compileProc(proc);
        T value[2];
        value[0] = 5;
        value[1] = 100;
        CHECK_EQ(invoke<T>(*code, value), oldValue(5, 1));
        CHECK_EQ(value[0], newValue(5, 1));
        CHECK_EQ(value[1], 100);
        checkMyDisassembly(*code, true);
    }

    {
        Procedure proc;
        BasicBlock* root = proc.addBlock();
        auto arguments = cCallArgumentValues<void*>(proc, root);
        root->appendNew<Value>(
            proc, Return, Origin(),
            root->appendNew<AtomicValue>(
                proc, opcode, Origin(), width,
                root->appendIntConstant(proc, Origin(), type, 42),
                arguments[0]));

        auto code = compileProc(proc);
        T value[2];
        value[0] = 5;
        value[1] = 100;
        CHECK_EQ(invoke<T>(*code, value), oldValue(5, 42));
        CHECK_EQ(value[0], newValue(5, 42));
        CHECK_EQ(value[1], 100);
        checkMyDisassembly(*code, true);
    }

    {
        Procedure proc;
        BasicBlock* root = proc.addBlock();
        auto arguments = cCallArgumentValues<void*>(proc, root);
        root->appendNew<AtomicValue>(
            proc, opcode, Origin(), width,
            root->appendIntConstant(proc, Origin(), type, 42),
            arguments[0]);
        root->appendNew<Value>(proc, Return, Origin());

        auto code = compileProc(proc);
        T value[2];
        value[0] = 5;
        value[1] = 100;
        invoke<T>(*code, value);
        CHECK_EQ(value[0], newValue(5, 42));
        CHECK_EQ(value[1], 100);
        checkMyDisassembly(*code, true);
    }

    {
        Procedure proc;
        BasicBlock* root = proc.addBlock();
        auto arguments = cCallArgumentValues<void*>(proc, root);
        root->appendNew<AtomicValue>(
            proc, opcode, Origin(), width,
            root->appendIntConstant(proc, Origin(), type, 42),
            arguments[0],
            0, HeapRange(42), HeapRange());
        root->appendNew<Value>(proc, Return, Origin());

        auto code = compileProc(proc);
        T value[2];
        value[0] = 5;
        value[1] = 100;
        invoke<T>(*code, value);
        CHECK_EQ(value[0], newValue(5, 42));
        CHECK_EQ(value[1], 100);
        checkMyDisassembly(*code, false);
    }
}

void addAtomicTests(const TestConfig* config, Deque<RefPtr<SharedTask<void()>>>& tasks)
{
    RUN(testAtomicWeakCAS<int8_t>());
    RUN(testAtomicWeakCAS<int16_t>());
    RUN(testAtomicWeakCAS<int32_t>());
    RUN(testAtomicWeakCAS<int64_t>());
    RUN(testAtomicStrongCAS<int8_t>());
    RUN(testAtomicStrongCAS<int16_t>());
    RUN(testAtomicStrongCAS<int32_t>());
    RUN(testAtomicStrongCAS<int64_t>());
    RUN(testAtomicXchg<int8_t>(AtomicXchgAdd));
    RUN(testAtomicXchg<int16_t>(AtomicXchgAdd));
    RUN(testAtomicXchg<int32_t>(AtomicXchgAdd));
    RUN(testAtomicXchg<int64_t>(AtomicXchgAdd));
    RUN(testAtomicXchg<int8_t>(AtomicXchgAnd));
    RUN(testAtomicXchg<int16_t>(AtomicXchgAnd));
    RUN(testAtomicXchg<int32_t>(AtomicXchgAnd));
    RUN(testAtomicXchg<int64_t>(AtomicXchgAnd));
    RUN(testAtomicXchg<int8_t>(AtomicXchgOr));
    RUN(testAtomicXchg<int16_t>(AtomicXchgOr));
    RUN(testAtomicXchg<int32_t>(AtomicXchgOr));
    RUN(testAtomicXchg<int64_t>(AtomicXchgOr));
    RUN(testAtomicXchg<int8_t>(AtomicXchgSub));
    RUN(testAtomicXchg<int16_t>(AtomicXchgSub));
    RUN(testAtomicXchg<int32_t>(AtomicXchgSub));
    RUN(testAtomicXchg<int64_t>(AtomicXchgSub));
    RUN(testAtomicXchg<int8_t>(AtomicXchgXor));
    RUN(testAtomicXchg<int16_t>(AtomicXchgXor));
    RUN(testAtomicXchg<int32_t>(AtomicXchgXor));
    RUN(testAtomicXchg<int64_t>(AtomicXchgXor));
    RUN(testAtomicXchg<int8_t>(AtomicXchg));
    RUN(testAtomicXchg<int16_t>(AtomicXchg));
    RUN(testAtomicXchg<int32_t>(AtomicXchg));
    RUN(testAtomicXchg<int64_t>(AtomicXchg));
}

template<typename CType, typename InputType>
void testLoad(B3::Type type, B3::Opcode opcode, InputType value)
{
    // Simple load from an absolute address.
    {
        Procedure proc;
        BasicBlock* root = proc.addBlock();

        root->clearSuccessors();
        root->appendNew<Value>(proc, Return, Origin(), root->appendNew<MemoryValue>(
                proc, opcode, type, Origin(),
                root->appendNew<ConstPtrValue>(proc, Origin(), &value)));

        CHECK(isIdentical(compileAndRun<CType>(proc), modelLoad<CType>(value)));
    }

    // Simple load from an address in a register.
    {
        Procedure proc;
        BasicBlock* root = proc.addBlock();
        auto arguments = cCallArgumentValues<void*>(proc, root);

        root->clearSuccessors();
        root->appendNew<Value>(proc, Return, Origin(), root->appendNew<MemoryValue>(
                proc, opcode, type, Origin(),
                arguments[0]));

        CHECK(isIdentical(compileAndRun<CType>(proc, &value), modelLoad<CType>(value)));
    }

    // Simple load from an address in a register, at an offset.
    {
        Procedure proc;
        BasicBlock* root = proc.addBlock();
        auto arguments = cCallArgumentValues<void*>(proc, root);

        root->clearSuccessors();
        root->appendNew<Value>(proc, Return, Origin(), root->appendNew<MemoryValue>(
                proc, opcode, type, Origin(),
                arguments[0],
                static_cast<int32_t>(sizeof(InputType))));

        CHECK(isIdentical(compileAndRun<CType>(proc, &value - 1), modelLoad<CType>(value)));
    }

    // Load from a simple base-index with various scales.
    for (unsigned logScale = 0; logScale <= 3; ++logScale) {
        Procedure proc;
        BasicBlock* root = proc.addBlock();
        auto arguments = cCallArgumentValues<void*, intptr_t>(proc, root);

        root->clearSuccessors();
        root->appendNew<Value>(proc, Return, Origin(), root->appendNew<MemoryValue>(
                proc, opcode, type, Origin(),
                root->appendNew<Value>(
                    proc, Add, Origin(),
                    arguments[0],
                    root->appendNew<Value>(
                        proc, Shl, Origin(),
                        arguments[1],
                        root->appendNew<Const32Value>(proc, Origin(), logScale)))));

        CHECK(isIdentical(compileAndRun<CType>(proc, &value - 2, (sizeof(InputType) * 2) >> logScale), modelLoad<CType>(value)));
    }

    // Load from a simple base-index with various scales, but commuted.
    for (unsigned logScale = 0; logScale <= 3; ++logScale) {
        Procedure proc;
        BasicBlock* root = proc.addBlock();
        auto arguments = cCallArgumentValues<void*, intptr_t>(proc, root);

        root->clearSuccessors();
        root->appendNew<Value>(proc, Return, Origin(), root->appendNew<MemoryValue>(
                proc, opcode, type, Origin(),
                root->appendNew<Value>(
                    proc, Add, Origin(),
                    root->appendNew<Value>(
                        proc, Shl, Origin(),
                        arguments[1],
                        root->appendNew<Const32Value>(proc, Origin(), logScale)),
                    arguments[0])));

        CHECK(isIdentical(compileAndRun<CType>(proc, &value - 2, (sizeof(InputType) * 2) >> logScale), modelLoad<CType>(value)));
    }
}

template<typename T>
void testLoad(B3::Opcode opcode, int32_t value)
{
    return testLoad<T>(B3::Int32, opcode, value);
}

template<typename T>
void testLoad(B3::Type type, T value)
{
    return testLoad<T>(type, Load, value);
}

void addLoadTests(const TestConfig* config, Deque<RefPtr<SharedTask<void()>>>& tasks)
{
    RUN(testLoad(Int32, 60));
    RUN(testLoad(Int32, -60));
    RUN(testLoad(Int32, 1000));
    RUN(testLoad(Int32, -1000));
    RUN(testLoad(Int32, 1000000));
    RUN(testLoad(Int32, -1000000));
    RUN(testLoad(Int32, 1000000000));
    RUN(testLoad(Int32, -1000000000));
    RUN_BINARY(testLoad, { MAKE_OPERAND(Int64) }, int64Operands());
    RUN_BINARY(testLoad, { MAKE_OPERAND(Float) }, floatingPointOperands<float>());
    RUN_BINARY(testLoad, { MAKE_OPERAND(Double) }, floatingPointOperands<double>());

    RUN(testLoad<int8_t>(Load8S, 60));
    RUN(testLoad<int8_t>(Load8S, -60));
    RUN(testLoad<int8_t>(Load8S, 1000));
    RUN(testLoad<int8_t>(Load8S, -1000));
    RUN(testLoad<int8_t>(Load8S, 1000000));
    RUN(testLoad<int8_t>(Load8S, -1000000));
    RUN(testLoad<int8_t>(Load8S, 1000000000));
    RUN(testLoad<int8_t>(Load8S, -1000000000));
    
    RUN(testLoad<uint8_t>(Load8Z, 60));
    RUN(testLoad<uint8_t>(Load8Z, -60));
    RUN(testLoad<uint8_t>(Load8Z, 1000));
    RUN(testLoad<uint8_t>(Load8Z, -1000));
    RUN(testLoad<uint8_t>(Load8Z, 1000000));
    RUN(testLoad<uint8_t>(Load8Z, -1000000));
    RUN(testLoad<uint8_t>(Load8Z, 1000000000));
    RUN(testLoad<uint8_t>(Load8Z, -1000000000));
    
    RUN(testLoad<int16_t>(Load16S, 60));
    RUN(testLoad<int16_t>(Load16S, -60));
    RUN(testLoad<int16_t>(Load16S, 1000));
    RUN(testLoad<int16_t>(Load16S, -1000));
    RUN(testLoad<int16_t>(Load16S, 1000000));
    RUN(testLoad<int16_t>(Load16S, -1000000));
    RUN(testLoad<int16_t>(Load16S, 1000000000));
    RUN(testLoad<int16_t>(Load16S, -1000000000));
    
    RUN(testLoad<uint16_t>(Load16Z, 60));
    RUN(testLoad<uint16_t>(Load16Z, -60));
    RUN(testLoad<uint16_t>(Load16Z, 1000));
    RUN(testLoad<uint16_t>(Load16Z, -1000));
    RUN(testLoad<uint16_t>(Load16Z, 1000000));
    RUN(testLoad<uint16_t>(Load16Z, -1000000));
    RUN(testLoad<uint16_t>(Load16Z, 1000000000));
    RUN(testLoad<uint16_t>(Load16Z, -1000000000));
}

void testWasmAddressDoesNotCSE()
{
    Procedure proc;
    GPRReg pinnedGPR = GPRInfo::argumentGPR0;
    proc.pinRegister(pinnedGPR);

    BasicBlock* root = proc.addBlock();
    BasicBlock* a = proc.addBlock();
    BasicBlock* b = proc.addBlock();
    BasicBlock* c = proc.addBlock();
    BasicBlock* continuation = proc.addBlock();

    auto* pointer = root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR1);
    auto* path = root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR2);

    auto* originalAddress = root->appendNew<WasmAddressValue>(proc, Origin(), pointer, pinnedGPR);
    root->appendNew<MemoryValue>(proc, Store, Origin(), originalAddress, 
        root->appendNew<WasmAddressValue>(proc, Origin(), root->appendNew<ConstPtrValue>(proc, Origin(), 6*8), pinnedGPR), 0);

    SwitchValue* switchValue = root->appendNew<SwitchValue>(proc, Origin(), path);
    switchValue->setFallThrough(FrequentedBlock(c));
    switchValue->appendCase(SwitchCase(0, FrequentedBlock(a)));
    switchValue->appendCase(SwitchCase(1, FrequentedBlock(b)));

    PatchpointValue* patchpoint = b->appendNew<PatchpointValue>(proc, Void, Origin());
    patchpoint->effects = Effects::forCall();
    patchpoint->clobber(RegisterSet::macroClobberedGPRs());
    patchpoint->clobber(RegisterSet(pinnedGPR));
    patchpoint->setGenerator(
        [&] (CCallHelpers& jit, const StackmapGenerationParams& params) {
            CHECK(!params.size());
            jit.addPtr(MacroAssembler::TrustedImm32(8), pinnedGPR);
        });

    UpsilonValue* takeA = a->appendNew<UpsilonValue>(proc, Origin(), a->appendNew<Const32Value>(proc, Origin(), 10));
    UpsilonValue* takeB = b->appendNew<UpsilonValue>(proc, Origin(), b->appendNew<Const32Value>(proc, Origin(), 20));
    UpsilonValue* takeC = c->appendNew<UpsilonValue>(proc, Origin(), c->appendNew<Const32Value>(proc, Origin(), 30));
    for (auto* i : { a, b, c }) {
        i->setSuccessors(FrequentedBlock(continuation));
        i->appendNew<Value>(proc, Jump, Origin());
        i->setSuccessors(FrequentedBlock(continuation));
    }

    // Continuation
    auto* takenPhi = continuation->appendNew<Value>(proc, Phi, Int32, Origin());

    auto* address2 = continuation->appendNew<WasmAddressValue>(proc, Origin(), pointer, pinnedGPR);
    continuation->appendNew<MemoryValue>(proc, Store, Origin(), takenPhi,
        continuation->appendNew<WasmAddressValue>(proc, Origin(), continuation->appendNew<ConstPtrValue>(proc, Origin(), 4*8), pinnedGPR),
        0);

    auto* returnVal = address2;
    continuation->clearSuccessors();
    continuation->appendNew<Value>(proc, Return, Origin(), returnVal);

    takeA->setPhi(takenPhi);
    takeB->setPhi(takenPhi);
    takeC->setPhi(takenPhi);

    auto binary = compileProc(proc);

    uint64_t* memory = new uint64_t[10];
    uintptr_t ptr = 8;

    uintptr_t finalPtr = reinterpret_cast<uintptr_t>(static_cast<void*>(memory)) + ptr;

    for (int i = 0; i < 10; ++i)
        memory[i] = 0;

    {
        uintptr_t result = invoke<uintptr_t>(*binary, memory, ptr, 0);

        CHECK_EQ(result, finalPtr);
        CHECK_EQ(memory[0], 0ul);
        CHECK_EQ(memory[1], 0ul);
        CHECK_EQ(memory[2], 0ul);
        CHECK_EQ(memory[4], 10ul);
        CHECK_EQ(memory[6], finalPtr);
    }

    memory[4] = 0;
    memory[5] = 0;
    memory[6] = 0;
    memory[7] = 0;

    {
        uintptr_t result = invoke<uintptr_t>(*binary, memory, ptr, 1);

        CHECK_EQ(result, finalPtr + 8);
        CHECK_EQ(memory[0], 0ul);
        CHECK_EQ(memory[1], 0ul);
        CHECK_EQ(memory[2], 0ul);
        CHECK_EQ(memory[5], 20ul);
        CHECK_EQ(memory[6], finalPtr);
    }

    memory[4] = 0;
    memory[5] = 0;
    memory[6] = 0;
    memory[7] = 0;
    {
        uintptr_t result = invoke<uintptr_t>(*binary, memory, ptr, 2);

        CHECK_EQ(result, finalPtr);
        CHECK_EQ(memory[0], 0ul);
        CHECK_EQ(memory[1], 0ul);
        CHECK_EQ(memory[2], 0ul);
        CHECK_EQ(memory[4], 30ul);
        CHECK_EQ(memory[6], finalPtr);
    }

    delete[] memory;
}

static unsigned countValuesWithOpcode(Procedure& proc, B3::Opcode opcode)
{
    unsigned count = 0;
    for (Value* value : proc.values()) {
        if (value->opcode() == opcode)
            ++count;
    }
    return count;
}

static void runReduceStrengthInitial(Procedure& proc)
{
    proc.resetReachability();
    if (shouldBeVerbose(proc)) {
        dataLog("IR before reduceStrength:\n");
        dataLog(proc);
    }
    reduceStrength(proc, ReduceStrengthPass::Initial);
    if (shouldBeVerbose(proc)) {
        dataLog("IR after reduceStrength:\n");
        dataLog(proc);
    }
}

static PatchpointValue* appendPinnedWrite(Procedure& proc, BasicBlock* block)
{
    PatchpointValue* patchpoint = block->appendNew<PatchpointValue>(proc, Void, Origin());
    patchpoint->effects = Effects::none();
    patchpoint->effects.writesPinned = true;
    return patchpoint;
}

void testReduceStrengthWasmAddressReuse()
{
    Procedure proc;
    if (proc.optLevel() < 2)
        return;
    GPRReg pinnedGPR = GPRInfo::argumentGPR0;
    proc.pinRegister(pinnedGPR);

    BasicBlock* root = proc.addBlock();
    Value* pointer = root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR1);
    Value* address1 = root->appendNew<WasmAddressValue>(proc, Origin(), pointer, pinnedGPR);
    Value* load1 = root->appendNew<MemoryValue>(proc, Load, Int32, Origin(), address1, 0);
    Value* address2 = root->appendNew<WasmAddressValue>(proc, Origin(), pointer, pinnedGPR);
    MemoryValue* load2 = root->appendNew<MemoryValue>(proc, Load, Int32, Origin(), address2, 4);
    root->clearSuccessors();
    root->appendNew<Value>(proc, Return, Origin(), root->appendNew<Value>(proc, Add, Origin(), load1, load2));

    runReduceStrengthInitial(proc);

    CHECK_EQ(countValuesWithOpcode(proc, WasmAddress), 1u);
    CHECK_EQ(load2->lastChild(), address1);
}

void testReduceStrengthWasmAddressAcrossDiamond()
{
    Procedure proc;
    if (proc.optLevel() < 2)
        return;
    GPRReg pinnedGPR = GPRInfo::argumentGPR0;
    proc.pinRegister(pinnedGPR);

    BasicBlock* root = proc.addBlock();
    BasicBlock* thenCase = proc.addBlock();
    BasicBlock* elseCase = proc.addBlock();
    BasicBlock* join = proc.addBlock();

    Value* pointer = root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR1);
    Value* other = root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR2);
    Value* condition = root->appendNew<Value>(proc, Trunc, Origin(), root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR3));
    Value* address1 = root->appendNew<WasmAddressValue>(proc, Origin(), pointer, pinnedGPR);
    Value* load1 = root->appendNew<MemoryValue>(proc, Load, Int32, Origin(), address1, 0);
    root->setSuccessors(FrequentedBlock(thenCase), FrequentedBlock(elseCase));
    root->appendNew<Value>(proc, Branch, Origin(), condition);

    thenCase->appendNew<MemoryValue>(proc, Store, Origin(), thenCase->appendNew<Const32Value>(proc, Origin(), 1), other, 0);
    thenCase->setSuccessors(FrequentedBlock(join));
    thenCase->appendNew<Value>(proc, Jump, Origin());
    elseCase->appendNew<MemoryValue>(proc, Store, Origin(), elseCase->appendNew<Const32Value>(proc, Origin(), 2), other, 0);
    elseCase->setSuccessors(FrequentedBlock(join));
    elseCase->appendNew<Value>(proc, Jump, Origin());

    Value* address2 = join->appendNew<WasmAddressValue>(proc, Origin(), pointer, pinnedGPR);
    MemoryValue* load2 = join->appendNew<MemoryValue>(proc, Load, Int32, Origin(), address2, 4);
    join->clearSuccessors();
    join->appendNew<Value>(proc, Return, Origin(), join->appendNew<Value>(proc, Add, Origin(), load1, load2));

    runReduceStrengthInitial(proc);

    CHECK_EQ(countValuesWithOpcode(proc, WasmAddress), 1u);
    CHECK_EQ(load2->lastChild(), address1);
}

void testReduceStrengthWasmAddressKilledByPinnedWrite()
{
    Procedure proc;
    if (proc.optLevel() < 2)
        return;
    GPRReg pinnedGPR = GPRInfo::argumentGPR0;
    proc.pinRegister(pinnedGPR);

    BasicBlock* root = proc.addBlock();
    BasicBlock* thenCase = proc.addBlock();
    BasicBlock* elseCase = proc.addBlock();
    BasicBlock* join = proc.addBlock();

    Value* pointer = root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR1);
    Value* other = root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR2);
    Value* condition = root->appendNew<Value>(proc, Trunc, Origin(), root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR3));
    Value* address1 = root->appendNew<WasmAddressValue>(proc, Origin(), pointer, pinnedGPR);
    Value* load1 = root->appendNew<MemoryValue>(proc, Load, Int32, Origin(), address1, 0);
    root->setSuccessors(FrequentedBlock(thenCase), FrequentedBlock(elseCase));
    root->appendNew<Value>(proc, Branch, Origin(), condition);

    thenCase->appendNew<MemoryValue>(proc, Store, Origin(), thenCase->appendNew<Const32Value>(proc, Origin(), 1), other, 0);
    thenCase->setSuccessors(FrequentedBlock(join));
    thenCase->appendNew<Value>(proc, Jump, Origin());
    appendPinnedWrite(proc, elseCase);
    elseCase->setSuccessors(FrequentedBlock(join));
    elseCase->appendNew<Value>(proc, Jump, Origin());

    Value* address2 = join->appendNew<WasmAddressValue>(proc, Origin(), pointer, pinnedGPR);
    MemoryValue* load2 = join->appendNew<MemoryValue>(proc, Load, Int32, Origin(), address2, 4);
    join->clearSuccessors();
    join->appendNew<Value>(proc, Return, Origin(), join->appendNew<Value>(proc, Add, Origin(), load1, load2));

    runReduceStrengthInitial(proc);

    CHECK_EQ(countValuesWithOpcode(proc, WasmAddress), 2u);
    CHECK_EQ(load2->lastChild(), address2);
}

static void testReduceStrengthWasmAddressLoopImpl(bool bodyWritesPinned)
{
    Procedure proc;
    if (proc.optLevel() < 2)
        return;
    GPRReg pinnedGPR = GPRInfo::argumentGPR0;
    proc.pinRegister(pinnedGPR);

    BasicBlock* root = proc.addBlock();
    BasicBlock* header = proc.addBlock();
    BasicBlock* body = proc.addBlock();
    BasicBlock* exit = proc.addBlock();

    Value* pointer = root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR1);
    Value* other = root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR2);
    Value* condition = root->appendNew<Value>(proc, Trunc, Origin(), root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR3));
    Value* address1 = root->appendNew<WasmAddressValue>(proc, Origin(), pointer, pinnedGPR);
    Value* load1 = root->appendNew<MemoryValue>(proc, Load, Int32, Origin(), address1, 0);
    root->setSuccessors(FrequentedBlock(header));
    root->appendNew<Value>(proc, Jump, Origin());

    Value* address2 = header->appendNew<WasmAddressValue>(proc, Origin(), pointer, pinnedGPR);
    MemoryValue* load2 = header->appendNew<MemoryValue>(proc, Load, Int32, Origin(), address2, 4);
    header->setSuccessors(FrequentedBlock(body), FrequentedBlock(exit));
    header->appendNew<Value>(proc, Branch, Origin(), condition);

    if (bodyWritesPinned)
        appendPinnedWrite(proc, body);
    else
        body->appendNew<MemoryValue>(proc, Store, Origin(), body->appendNew<Const32Value>(proc, Origin(), 1), other, 0);
    body->setSuccessors(FrequentedBlock(header));
    body->appendNew<Value>(proc, Jump, Origin());

    exit->clearSuccessors();
    exit->appendNew<Value>(proc, Return, Origin(), exit->appendNew<Value>(proc, Add, Origin(), load1, load2));

    runReduceStrengthInitial(proc);

    if (bodyWritesPinned) {
        CHECK_EQ(countValuesWithOpcode(proc, WasmAddress), 2u);
        CHECK_EQ(load2->lastChild(), address2);
    } else {
        CHECK_EQ(countValuesWithOpcode(proc, WasmAddress), 1u);
        CHECK_EQ(load2->lastChild(), address1);
    }
}

void testReduceStrengthWasmAddressLoop()
{
    testReduceStrengthWasmAddressLoopImpl(true);
    testReduceStrengthWasmAddressLoopImpl(false);
}

void testReduceStrengthWasmAddressDifferentPinnedGPR()
{
    Procedure proc;
    if (proc.optLevel() < 2)
        return;
    GPRReg pinnedGPR1 = GPRInfo::argumentGPR0;
    GPRReg pinnedGPR2 = GPRInfo::argumentGPR3;
    proc.pinRegister(pinnedGPR1);
    proc.pinRegister(pinnedGPR2);

    BasicBlock* root = proc.addBlock();
    Value* pointer = root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR1);
    Value* address1 = root->appendNew<WasmAddressValue>(proc, Origin(), pointer, pinnedGPR1);
    Value* load1 = root->appendNew<MemoryValue>(proc, Load, Int32, Origin(), address1, 0);
    Value* address2 = root->appendNew<WasmAddressValue>(proc, Origin(), pointer, pinnedGPR2);
    MemoryValue* load2 = root->appendNew<MemoryValue>(proc, Load, Int32, Origin(), address2, 4);
    root->clearSuccessors();
    root->appendNew<Value>(proc, Return, Origin(), root->appendNew<Value>(proc, Add, Origin(), load1, load2));

    runReduceStrengthInitial(proc);

    CHECK_EQ(countValuesWithOpcode(proc, WasmAddress), 2u);
    CHECK_EQ(load2->lastChild(), address2);
}

static MemoryValue* appendLoadWithRange(Procedure& proc, BasicBlock* block, Kind kind, Type type, Value* pointer, HeapRange range, int32_t offset = 0)
{
    MemoryValue* load = block->appendNew<MemoryValue>(proc, kind, type, Origin(), pointer, offset);
    load->setRange(range);
    return load;
}

static MemoryValue* appendStoreWithRange(Procedure& proc, BasicBlock* block, Kind kind, Value* value, Value* pointer, HeapRange range, int32_t offset = 0)
{
    return block->appendNew<MemoryValue>(proc, kind, Origin(), value, pointer, offset, range);
}

// Keeps `value` alive through a store whose range no test load overlaps.
static void appendSink(Procedure& proc, BasicBlock* block, Value* sink, Value* value, int32_t offset = 0)
{
    appendStoreWithRange(proc, block, Store, value, sink, HeapRange(1000, 1001), offset);
}

static Value* returnedValue(BasicBlock* block)
{
    CHECK_EQ(block->last()->opcode(), Return);
    return block->last()->child(0);
}

// Loads p twice through `range`, with `between` appended in the middle, and returns whether the
// second load was replaced by the first.
template<typename Between>
static bool secondLoadReusesFirst(HeapRange range, const Between& between)
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    Value* pointer = root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0);
    Value* other = root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR1);
    Value* sink = root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR2);
    MemoryValue* load1 = appendLoadWithRange(proc, root, Load, Int32, pointer, range);
    appendSink(proc, root, sink, load1);
    between(proc, root, other);
    MemoryValue* load2 = appendLoadWithRange(proc, root, Load, Int32, pointer, range);
    root->clearSuccessors();
    root->appendNew<Value>(proc, Return, Origin(), load2);

    runReduceStrengthInitial(proc);

    Value* result = returnedValue(root);
    CHECK(result == load1 || result == load2);
    return result == load1;
}

void testReduceStrengthLoadLoadSameBlock()
{
    if (Options::defaultB3OptLevel() < 2)
        return;
    CHECK(secondLoadReusesFirst(HeapRange(10, 20), [](Procedure&, BasicBlock*, Value*) { }));
}

void testReduceStrengthLoadLoadAcrossDiamond()
{
    Procedure proc;
    if (proc.optLevel() < 2)
        return;

    BasicBlock* root = proc.addBlock();
    BasicBlock* thenCase = proc.addBlock();
    BasicBlock* elseCase = proc.addBlock();
    BasicBlock* join = proc.addBlock();

    Value* pointer = root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0);
    Value* sink = root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR2);
    Value* condition = root->appendNew<Value>(proc, Trunc, Origin(), root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR3));
    MemoryValue* load1 = appendLoadWithRange(proc, root, Load, Int32, pointer, HeapRange(10, 20));
    appendSink(proc, root, sink, load1);
    root->setSuccessors(FrequentedBlock(thenCase), FrequentedBlock(elseCase));
    root->appendNew<Value>(proc, Branch, Origin(), condition);

    appendSink(proc, thenCase, sink, thenCase->appendNew<Const32Value>(proc, Origin(), 1), 4);
    thenCase->setSuccessors(FrequentedBlock(join));
    thenCase->appendNew<Value>(proc, Jump, Origin());
    appendSink(proc, elseCase, sink, elseCase->appendNew<Const32Value>(proc, Origin(), 2), 4);
    elseCase->setSuccessors(FrequentedBlock(join));
    elseCase->appendNew<Value>(proc, Jump, Origin());

    MemoryValue* load2 = appendLoadWithRange(proc, join, Load, Int32, pointer, HeapRange(10, 20));
    join->clearSuccessors();
    join->appendNew<Value>(proc, Return, Origin(), load2);

    runReduceStrengthInitial(proc);

    CHECK_EQ(returnedValue(join), load1);
}

void testReduceStrengthLoadKilledOnOneArm()
{
    Procedure proc;
    if (proc.optLevel() < 2)
        return;

    BasicBlock* root = proc.addBlock();
    BasicBlock* thenCase = proc.addBlock();
    BasicBlock* elseCase = proc.addBlock();
    BasicBlock* join = proc.addBlock();

    Value* pointer = root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0);
    Value* other = root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR1);
    Value* sink = root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR2);
    Value* condition = root->appendNew<Value>(proc, Trunc, Origin(), root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR3));
    MemoryValue* load1 = appendLoadWithRange(proc, root, Load, Int32, pointer, HeapRange(10, 20));
    appendSink(proc, root, sink, load1);
    root->setSuccessors(FrequentedBlock(thenCase), FrequentedBlock(elseCase));
    root->appendNew<Value>(proc, Branch, Origin(), condition);

    appendStoreWithRange(proc, thenCase, Store, thenCase->appendNew<Const32Value>(proc, Origin(), 1), other, HeapRange(15, 16));
    thenCase->setSuccessors(FrequentedBlock(join));
    thenCase->appendNew<Value>(proc, Jump, Origin());
    appendSink(proc, elseCase, sink, elseCase->appendNew<Const32Value>(proc, Origin(), 2), 4);
    elseCase->setSuccessors(FrequentedBlock(join));
    elseCase->appendNew<Value>(proc, Jump, Origin());

    MemoryValue* load2 = appendLoadWithRange(proc, join, Load, Int32, pointer, HeapRange(10, 20));
    join->clearSuccessors();
    join->appendNew<Value>(proc, Return, Origin(), load2);

    runReduceStrengthInitial(proc);

    CHECK_EQ(returnedValue(join), load2);
}

void testReduceStrengthLoadKilledByNestedWrite()
{
    if (Options::defaultB3OptLevel() < 2)
        return;

    auto storeTo = [](HeapRange range) {
        return [=](Procedure& proc, BasicBlock* block, Value* other) {
            appendStoreWithRange(proc, block, Store, block->appendNew<Const32Value>(proc, Origin(), 1), other, range);
        };
    };

    CHECK(!secondLoadReusesFirst(HeapRange(0, 100), storeTo(HeapRange(10, 20))));
    CHECK(!secondLoadReusesFirst(HeapRange(10, 20), storeTo(HeapRange(0, 100))));
}

void testReduceStrengthLoadSurvivesDisjointWrite()
{
    if (Options::defaultB3OptLevel() < 2)
        return;

    auto storeToSiblings = [](Procedure& proc, BasicBlock* block, Value* other) {
        appendSink(proc, block, other, appendLoadWithRange(proc, block, Load, Int32, other, HeapRange(0, 100)), 8);
        appendStoreWithRange(proc, block, Store, block->appendNew<Const32Value>(proc, Origin(), 1), other, HeapRange(20, 30));
        appendStoreWithRange(proc, block, Store, block->appendNew<Const32Value>(proc, Origin(), 2), other, HeapRange(0, 10));
    };
    CHECK(secondLoadReusesFirst(HeapRange(10, 20), storeToSiblings));

    auto writeOutsideEveryAccess = [](Procedure& proc, BasicBlock* block, Value*) {
        PatchpointValue* patchpoint = block->appendNew<PatchpointValue>(proc, Void, Origin());
        patchpoint->effects = Effects::none();
        patchpoint->effects.writes = HeapRange(5000, 6000);
    };
    CHECK(secondLoadReusesFirst(HeapRange(10, 20), writeOutsideEveryAccess));
}

void testReduceStrengthLoadKilledByPartialOverlapWrite()
{
    if (Options::defaultB3OptLevel() < 2)
        return;

    // HeapRange(0, 10) becomes a node and HeapRange(5, 15), which partially overlaps it, does not.
    auto loadThenStoreTo = [](HeapRange range) {
        return [=](Procedure& proc, BasicBlock* block, Value* other) {
            appendSink(proc, block, other, appendLoadWithRange(proc, block, Load, Int32, other, HeapRange(0, 10)), 8);
            appendStoreWithRange(proc, block, Store, block->appendNew<Const32Value>(proc, Origin(), 1), other, range);
        };
    };
    CHECK(!secondLoadReusesFirst(HeapRange(0, 10), loadThenStoreTo(HeapRange(5, 15))));
    CHECK(!secondLoadReusesFirst(HeapRange(5, 15), loadThenStoreTo(HeapRange(0, 10))));

    // A write of a range that no access mentions, but that overlaps the part of HeapRange(5, 15)
    // that no node covers.
    auto loadThenWriteOutsideNodes = [](Procedure& proc, BasicBlock* block, Value* other) {
        appendSink(proc, block, other, appendLoadWithRange(proc, block, Load, Int32, other, HeapRange(0, 10)), 8);
        PatchpointValue* patchpoint = block->appendNew<PatchpointValue>(proc, Void, Origin());
        patchpoint->effects = Effects::none();
        patchpoint->effects.writes = HeapRange(12, 13);
    };
    CHECK(!secondLoadReusesFirst(HeapRange(5, 15), loadThenWriteOutsideNodes));
}

void testReduceStrengthLoadFromStoreOfDifferentRange()
{
    Procedure proc;
    if (proc.optLevel() < 2)
        return;

    // The store and the load name the same address through different nodes, so their epochs are
    // sums over different counters and can coincide even though a write overlaps the load.
    BasicBlock* root = proc.addBlock();
    Value* pointer = root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0);
    Value* other = root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR1);
    Value* stored = root->appendNew<Value>(proc, Trunc, Origin(), root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR2));
    appendStoreWithRange(proc, root, Store, stored, pointer, HeapRange(0, 100));
    appendStoreWithRange(proc, root, Store, root->appendNew<Const32Value>(proc, Origin(), 1), other, HeapRange(12, 13));
    MemoryValue* load = appendLoadWithRange(proc, root, Load, Int32, pointer, HeapRange(10, 20));
    root->clearSuccessors();
    root->appendNew<Value>(proc, Return, Origin(), load);

    runReduceStrengthInitial(proc);

    CHECK_EQ(returnedValue(root), load);
}

// Stores the argument through `storeKind` and loads it back through `loadKind`, checking that the
// load becomes `expectedOpcode` applied to the stored value and that the compiled code agrees.
static void testStoreToLoadFixup(Kind storeKind, Kind loadKind, Type loadType, int64_t input, int64_t expected, B3::Opcode expectedOpcode, int32_t expectedMask)
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    Value* pointer = root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0);
    Value* stored = root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR1);
    if (storeKind.opcode() != Store)
        stored = root->appendNew<Value>(proc, Trunc, Origin(), stored);
    MemoryValue* store = appendStoreWithRange(proc, root, storeKind, stored, pointer, HeapRange(10, 20));
    MemoryValue* load = appendLoadWithRange(proc, root, loadKind, loadType, pointer, HeapRange(10, 20));
    root->clearSuccessors();
    root->appendNew<Value>(proc, Return, Origin(), load);

    runReduceStrengthInitial(proc);

    Value* result = returnedValue(root);
    CHECK_EQ(result->opcode(), expectedOpcode);
    CHECK_EQ(result->child(0), store->child(0));
    if (expectedOpcode == BitAnd)
        CHECK(result->child(1)->isInt32(expectedMask));

    int64_t storage = 0;
    CHECK_EQ(static_cast<int64_t>(invoke<int32_t>(*compileProc(proc), &storage, input)), expected);
}

void testReduceStrengthStoreToLoadFixups()
{
    if (Options::defaultB3OptLevel() < 2)
        return;

    testStoreToLoadFixup(Store8, Load8Z, Int32, 0x12345680, 0x80, BitAnd, 0xff);
    testStoreToLoadFixup(Store8, Load8S, Int32, 0x12345680, -0x80, SExt8, 0);
    testStoreToLoadFixup(Store16, Load16Z, Int32, 0x12348765, 0x8765, BitAnd, 0xffff);
    testStoreToLoadFixup(Store16, Load16S, Int32, 0x12348765, static_cast<int16_t>(0x8765), SExt16, 0);
    testStoreToLoadFixup(Store, Load, Int32, 0x1234567887654321, static_cast<int32_t>(0x87654321), Trunc, 0);

    // An Int64 load followed by an Int32 load of the same location.
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    Value* pointer = root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0);
    Value* sink = root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR1);
    MemoryValue* wide = appendLoadWithRange(proc, root, Load, Int64, pointer, HeapRange(10, 20));
    appendSink(proc, root, sink, wide);
    MemoryValue* narrow = appendLoadWithRange(proc, root, Load, Int32, pointer, HeapRange(10, 20));
    root->clearSuccessors();
    root->appendNew<Value>(proc, Return, Origin(), narrow);

    runReduceStrengthInitial(proc);

    Value* result = returnedValue(root);
    CHECK_EQ(result->opcode(), Trunc);
    CHECK_EQ(result->child(0), wide);

    int64_t storage = 0x1234567887654321;
    int64_t sinkStorage = 0;
    CHECK_EQ(invoke<int32_t>(*compileProc(proc), &storage, &sinkStorage), static_cast<int32_t>(0x87654321));
    CHECK_EQ(sinkStorage, storage);
}

void testReduceStrengthRedundantStore()
{
    Procedure proc;
    if (proc.optLevel() < 2)
        return;

    BasicBlock* root = proc.addBlock();
    Value* pointer = root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0);
    Value* stored = root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR1);
    Value* otherPointer = root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR2);
    appendStoreWithRange(proc, root, Store, stored, pointer, HeapRange(10, 20));
    appendStoreWithRange(proc, root, Store, stored, pointer, HeapRange(10, 20));
    MemoryValue* loaded = appendLoadWithRange(proc, root, Load, Int64, otherPointer, HeapRange(30, 40));
    appendStoreWithRange(proc, root, Store, loaded, otherPointer, HeapRange(30, 40));
    appendStoreWithRange(proc, root, trapping(Store), stored, pointer, HeapRange(10, 20));
    root->clearSuccessors();
    root->appendNew<Value>(proc, Return, Origin());

    runReduceStrengthInitial(proc);

    unsigned stores = 0;
    unsigned trappingStores = 0;
    for (Value* value : *root) {
        if (value->opcode() != Store)
            continue;
        ++stores;
        if (value->traps())
            ++trappingStores;
    }
    CHECK_EQ(stores, 2u);
    CHECK_EQ(trappingStores, 1u);
}

void testReduceStrengthImmutableLoadKeptAcrossCall()
{
    if (Options::defaultB3OptLevel() < 2)
        return;

    Procedure proc;
    BasicBlock* root = proc.addBlock();
    Value* pointer = root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0);
    Value* sink = root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR2);
    MemoryValue* load1 = appendLoadWithRange(proc, root, Load, Int32, pointer, HeapRange(10, 20));
    load1->setReadsMutability(Mutability::Immutable);
    appendSink(proc, root, sink, load1);
    PatchpointValue* call = root->appendNew<PatchpointValue>(proc, Void, Origin());
    call->effects = Effects::forCall();
    MemoryValue* load2 = appendLoadWithRange(proc, root, Load, Int32, pointer, HeapRange(10, 20));
    load2->setReadsMutability(Mutability::Immutable);
    root->clearSuccessors();
    root->appendNew<Value>(proc, Return, Origin(), load2);

    runReduceStrengthInitial(proc);

    CHECK_EQ(returnedValue(root), load2);
}

void testReduceStrengthImmutableLoadNotFromMutableAcrossStore()
{
    if (Options::defaultB3OptLevel() < 2)
        return;

    Procedure proc;
    BasicBlock* root = proc.addBlock();
    Value* pointer = root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0);
    Value* stored = root->appendNew<Value>(proc, Trunc, Origin(), root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR1));
    Value* sink = root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR2);
    MemoryValue* load1 = appendLoadWithRange(proc, root, Load, Int32, pointer, HeapRange(10, 20));
    appendSink(proc, root, sink, load1);
    appendStoreWithRange(proc, root, Store, stored, pointer, HeapRange(10, 20));
    MemoryValue* load2 = appendLoadWithRange(proc, root, Load, Int32, pointer, HeapRange(10, 20));
    load2->setReadsMutability(Mutability::Immutable);
    root->clearSuccessors();
    root->appendNew<Value>(proc, Return, Origin(), load2);

    runReduceStrengthInitial(proc);

    CHECK(returnedValue(root) != load1);

    int32_t storage = 1;
    int32_t sinkStorage = 0;
    CHECK_EQ(invoke<int32_t>(*compileProc(proc), &storage, 2, &sinkStorage), 2);
    CHECK_EQ(sinkStorage, 1);
}

void testReduceStrengthMutableLoadNotFromImmutableAcrossCall()
{
    if (Options::defaultB3OptLevel() < 2)
        return;

    Procedure proc;
    BasicBlock* root = proc.addBlock();
    Value* pointer = root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0);
    Value* sink = root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR2);
    MemoryValue* load1 = appendLoadWithRange(proc, root, Load, Int32, pointer, HeapRange(10, 20));
    load1->setReadsMutability(Mutability::Immutable);
    appendSink(proc, root, sink, load1);
    PatchpointValue* call = root->appendNew<PatchpointValue>(proc, Void, Origin());
    call->effects = Effects::forCall();
    MemoryValue* load2 = appendLoadWithRange(proc, root, Load, Int32, pointer, HeapRange(10, 20));
    root->clearSuccessors();
    root->appendNew<Value>(proc, Return, Origin(), load2);

    runReduceStrengthInitial(proc);

    CHECK_EQ(returnedValue(root), load2);
}

void testReduceStrengthLoadsAcrossLoweredMemoryFill()
{
    if (Options::defaultB3OptLevel() < 2)
        return;

    Procedure proc;
    BasicBlock* root = proc.addBlock();
    Value* pointer = root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0);
    Value* sink = root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR1);
    MemoryValue* load1 = appendLoadWithRange(proc, root, Load, Int32, pointer, HeapRange(10, 20));
    appendSink(proc, root, sink, load1);
    BulkMemoryValue* fill = root->appendNew<BulkMemoryValue>(proc, MemoryFill, Origin(), pointer, root->appendNew<Const32Value>(proc, Origin(), 0xab), root->appendIntConstant(proc, Origin(), pointerType(), 4));
    fill->setWriteRange(HeapRange(10, 20));
    MemoryValue* load2 = appendLoadWithRange(proc, root, Load, Int32, pointer, HeapRange(10, 20));
    root->clearSuccessors();
    root->appendNew<Value>(proc, Return, Origin(), load2);

    runReduceStrengthInitial(proc);

    CHECK_EQ(countValuesWithOpcode(proc, MemoryFill), 0u);
    CHECK(returnedValue(root) != load1);

    int32_t storage = 5;
    int32_t sinkStorage = 0;
    CHECK_EQ(invoke<int32_t>(*compileProc(proc), &storage, &sinkStorage), static_cast<int32_t>(0xabababab));
    CHECK_EQ(sinkStorage, 5);
    CHECK_EQ(storage, static_cast<int32_t>(0xabababab));
}

// Counts down from the argument, loading p in the loop header and storing to p (or to a disjoint
// range of another pointer) in the body. Returns the header load minus the load before the loop.
static void testReduceStrengthLoadLoopHeaderImpl(bool bodyWritesLoadedRange)
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    BasicBlock* header = proc.addBlock();
    BasicBlock* body = proc.addBlock();
    BasicBlock* exit = proc.addBlock();

    Value* pointer = root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0);
    Value* other = root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR1);
    Value* tripCount = root->appendNew<Value>(proc, Trunc, Origin(), root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR2));
    MemoryValue* before = appendLoadWithRange(proc, root, Load, Int32, pointer, HeapRange(10, 20));
    UpsilonValue* initialCounter = root->appendNew<UpsilonValue>(proc, Origin(), tripCount);
    root->setSuccessors(FrequentedBlock(header));
    root->appendNew<Value>(proc, Jump, Origin());

    Value* counter = header->appendNew<Value>(proc, Phi, Int32, Origin());
    initialCounter->setPhi(counter);
    MemoryValue* inLoop = appendLoadWithRange(proc, header, Load, Int32, pointer, HeapRange(10, 20));
    header->setSuccessors(FrequentedBlock(body), FrequentedBlock(exit));
    header->appendNew<Value>(proc, Branch, Origin(), counter);

    Value* incremented = body->appendNew<Value>(proc, Add, Origin(), inLoop, body->appendNew<Const32Value>(proc, Origin(), 1));
    if (bodyWritesLoadedRange)
        appendStoreWithRange(proc, body, Store, incremented, pointer, HeapRange(10, 20));
    else
        appendStoreWithRange(proc, body, Store, incremented, other, HeapRange(20, 30));
    Value* nextCounter = body->appendNew<Value>(proc, Sub, Origin(), counter, body->appendNew<Const32Value>(proc, Origin(), 1));
    body->appendNew<UpsilonValue>(proc, Origin(), nextCounter)->setPhi(counter);
    body->setSuccessors(FrequentedBlock(header));
    body->appendNew<Value>(proc, Jump, Origin());

    exit->clearSuccessors();
    exit->appendNew<Value>(proc, Return, Origin(), exit->appendNew<Value>(proc, Sub, Origin(), inLoop, before));

    runReduceStrengthInitial(proc);

    CHECK_EQ(countValuesWithOpcode(proc, Load), (bodyWritesLoadedRange ? 2u : 1u));

    int32_t storage = 100;
    int32_t otherStorage = 0;
    int32_t result = invoke<int32_t>(*compileProc(proc), &storage, &otherStorage, 5);
    if (bodyWritesLoadedRange) {
        CHECK_EQ(result, 5);
        CHECK_EQ(storage, 105);
    } else {
        CHECK_EQ(result, 0);
        CHECK_EQ(otherStorage, 101);
    }
}

void testReduceStrengthLoadLoopHeader()
{
    if (Options::defaultB3OptLevel() < 2)
        return;
    testReduceStrengthLoadLoopHeaderImpl(true);
    testReduceStrengthLoadLoopHeaderImpl(false);
}

void testReduceStrengthFencedLoadNotForwarded()
{
    if (Options::defaultB3OptLevel() < 2)
        return;

    for (bool fenceFirst : { false, true }) {
        Procedure proc;
        BasicBlock* root = proc.addBlock();
        Value* pointer = root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0);
        Value* sink = root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR2);
        HeapRange range(10, 20);
        HeapRange fence(2000, 2001);
        MemoryValue* load1 = root->appendNew<MemoryValue>(proc, Load, Int32, Origin(), pointer, 0, range, fenceFirst ? fence : HeapRange());
        appendSink(proc, root, sink, load1);
        MemoryValue* load2 = root->appendNew<MemoryValue>(proc, Load, Int32, Origin(), pointer, 0, range, fenceFirst ? HeapRange() : fence);
        root->clearSuccessors();
        root->appendNew<Value>(proc, Return, Origin(), load2);

        runReduceStrengthInitial(proc);

        CHECK_EQ(returnedValue(root), load2);
    }
}

void testReduceStrengthLoadIrreducibleEntry()
{
    Procedure proc;
    if (proc.optLevel() < 2)
        return;

    BasicBlock* root = proc.addBlock();
    BasicBlock* first = proc.addBlock();
    BasicBlock* second = proc.addBlock();
    BasicBlock* exit = proc.addBlock();

    Value* pointer = root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0);
    Value* other = root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR1);
    Value* sink = root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR2);
    Value* condition = root->appendNew<Value>(proc, Trunc, Origin(), root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR3));
    MemoryValue* before = appendLoadWithRange(proc, root, Load, Int32, pointer, HeapRange(10, 20));
    appendSink(proc, root, sink, before);
    root->setSuccessors(FrequentedBlock(first), FrequentedBlock(second));
    root->appendNew<Value>(proc, Branch, Origin(), condition);

    MemoryValue* inFirst = appendLoadWithRange(proc, first, Load, Int32, pointer, HeapRange(10, 20));
    MemoryValue* sinkFirst = appendStoreWithRange(proc, first, Store, inFirst, sink, HeapRange(1000, 1001), 4);
    first->setSuccessors(FrequentedBlock(second), FrequentedBlock(exit));
    first->appendNew<Value>(proc, Branch, Origin(), condition);

    MemoryValue* inSecond = appendLoadWithRange(proc, second, Load, Int32, pointer, HeapRange(10, 20));
    MemoryValue* sinkSecond = appendStoreWithRange(proc, second, Store, inSecond, sink, HeapRange(1000, 1001), 8);
    appendStoreWithRange(proc, second, Store, second->appendNew<Const32Value>(proc, Origin(), 1), other, HeapRange(10, 20));
    second->setSuccessors(FrequentedBlock(first));
    second->appendNew<Value>(proc, Jump, Origin());

    exit->clearSuccessors();
    exit->appendNew<Value>(proc, Return, Origin());

    runReduceStrengthInitial(proc);

    CHECK_EQ(sinkFirst->child(0), inFirst);
    CHECK_EQ(sinkSecond->child(0), inSecond);
}

void testReduceStrengthLoadThroughReusedWasmAddress()
{
    Procedure proc;
    if (proc.optLevel() < 2)
        return;
    GPRReg pinnedGPR = GPRInfo::argumentGPR0;
    proc.pinRegister(pinnedGPR);

    BasicBlock* root = proc.addBlock();
    Value* pointer = root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR1);
    Value* sink = root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR2);
    Value* address1 = root->appendNew<WasmAddressValue>(proc, Origin(), pointer, pinnedGPR);
    MemoryValue* load1 = appendLoadWithRange(proc, root, Load, Int32, address1, HeapRange(10, 20));
    appendSink(proc, root, sink, load1);
    Value* address2 = root->appendNew<WasmAddressValue>(proc, Origin(), pointer, pinnedGPR);
    MemoryValue* load2 = appendLoadWithRange(proc, root, Load, Int32, address2, HeapRange(10, 20));
    root->clearSuccessors();
    root->appendNew<Value>(proc, Return, Origin(), load2);

    runReduceStrengthInitial(proc);

    CHECK_EQ(returnedValue(root), load1);
}

// The branch folds while the walk is in progress, which leaves the arm it no longer takes
// unreachable. What that arm writes must not keep the join from forwarding, and what the taken
// arm writes must keep the join from reusing the load before the branch.
static void testReduceStrengthLoadForwardingAfterCFGFoldImpl(bool takeStoringArm)
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    BasicBlock* storing = proc.addBlock();
    BasicBlock* notStoring = proc.addBlock();
    BasicBlock* join = proc.addBlock();

    Value* pointer = root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0);
    Value* sink = root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR2);
    MemoryValue* before = appendLoadWithRange(proc, root, Load, Int32, pointer, HeapRange(10, 20));
    appendSink(proc, root, sink, before);
    root->setSuccessors(FrequentedBlock(storing), FrequentedBlock(notStoring));
    root->appendNew<Value>(proc, Branch, Origin(), root->appendNew<Const32Value>(proc, Origin(), takeStoringArm));

    appendStoreWithRange(proc, storing, Store, storing->appendNew<Const32Value>(proc, Origin(), 42), pointer, HeapRange(10, 20));
    storing->setSuccessors(FrequentedBlock(join));
    storing->appendNew<Value>(proc, Jump, Origin());
    appendSink(proc, notStoring, sink, notStoring->appendNew<Const32Value>(proc, Origin(), 1), 4);
    notStoring->setSuccessors(FrequentedBlock(join));
    notStoring->appendNew<Value>(proc, Jump, Origin());

    MemoryValue* after = appendLoadWithRange(proc, join, Load, Int32, pointer, HeapRange(10, 20));
    join->clearSuccessors();
    join->appendNew<Value>(proc, Return, Origin(), after);

    runReduceStrengthInitial(proc);

    Value* result = returnedValue(root);
    if (takeStoringArm)
        CHECK(result->isInt32(42) || result == after);
    else
        CHECK_EQ(result, before);
}

void testReduceStrengthLoadForwardingAfterCFGFold()
{
    if (Options::defaultB3OptLevel() < 2)
        return;
    testReduceStrengthLoadForwardingAfterCFGFoldImpl(true);
    testReduceStrengthLoadForwardingAfterCFGFoldImpl(false);
}

static Vector<Value*> valuesStoredThrough(Procedure& proc, Value* pointer)
{
    Vector<Value*> result;
    for (BasicBlock* block : proc) {
        for (Value* value : *block) {
            if (MemoryValue* memory = value->as<MemoryValue>(); memory && memory->isStore() && memory->lastChild() == pointer)
                result.append(memory->child(0));
        }
    }
    return result;
}

// Stores a and then b to p, with `between` appended in the middle, and returns how many stores to
// p remain after reduction.
template<typename Between>
static size_t deadStoreSurvivors(Kind firstKind, Kind secondKind, const Between& between)
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    Value* pointer = root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0);
    Value* other = root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR1);
    Value* a = root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR2);
    Value* b = root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR3);
    appendStoreWithRange(proc, root, firstKind, a, pointer, HeapRange(10, 20));
    between(proc, root, other);
    appendStoreWithRange(proc, root, secondKind, b, pointer, HeapRange(10, 20));
    root->clearSuccessors();
    root->appendNew<Value>(proc, Return, Origin());

    runReduceStrengthInitial(proc);

    return valuesStoredThrough(proc, pointer).size();
}

static auto nothingBetween = [](Procedure&, BasicBlock*, Value*) { };

void testReduceStrengthDeadStoreSameBlock()
{
    if (Options::defaultB3OptLevel() < 2)
        return;

    CHECK_EQ(deadStoreSurvivors(Store, Store, nothingBetween), 1u);

    Procedure proc;
    BasicBlock* root = proc.addBlock();
    Value* pointer = root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0);
    Value* a = root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR1);
    Value* b = root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR2);
    appendStoreWithRange(proc, root, Store, a, pointer, HeapRange(10, 20));
    appendStoreWithRange(proc, root, Store, b, pointer, HeapRange(10, 20));
    root->clearSuccessors();
    root->appendNew<Value>(proc, Return, Origin());

    runReduceStrengthInitial(proc);

    Vector<Value*> survivors = valuesStoredThrough(proc, pointer);
    CHECK_EQ(survivors.size(), 1u);
    CHECK_EQ(survivors[0], b);

    int64_t storage = 0;
    invoke<void>(*compileProc(proc), &storage, static_cast<int64_t>(1), static_cast<int64_t>(2));
    CHECK_EQ(storage, 2);
}

void testReduceStrengthDeadStoreThenLoadForwardsKiller()
{
    if (Options::defaultB3OptLevel() < 2)
        return;

    Procedure proc;
    BasicBlock* root = proc.addBlock();
    Value* pointer = root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0);
    Value* a = root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR1);
    Value* b = root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR2);
    appendStoreWithRange(proc, root, Store, a, pointer, HeapRange(10, 20));
    appendStoreWithRange(proc, root, Store, b, pointer, HeapRange(10, 20));
    MemoryValue* load = appendLoadWithRange(proc, root, Load, Int64, pointer, HeapRange(10, 20));
    root->clearSuccessors();
    root->appendNew<Value>(proc, Return, Origin(), load);

    runReduceStrengthInitial(proc);

    Vector<Value*> survivors = valuesStoredThrough(proc, pointer);
    CHECK_EQ(survivors.size(), 1u);
    CHECK_EQ(survivors[0], b);
    CHECK_EQ(returnedValue(root), b);

    int64_t storage = 0;
    CHECK_EQ(invoke<int64_t>(*compileProc(proc), &storage, static_cast<int64_t>(1), static_cast<int64_t>(2)), 2);
    CHECK_EQ(storage, 2);
}

void testReduceStrengthDeadStoreKeptAcrossFence()
{
    if (Options::defaultB3OptLevel() < 2)
        return;

    auto fence = [](Procedure& proc, BasicBlock* block, Value*) {
        block->appendNew<FenceValue>(proc, Origin(), HeapRange(2000, 2001), HeapRange(2000, 2001));
    };
    CHECK_EQ(deadStoreSurvivors(Store, Store, fence), 2u);

    auto fencedLoad = [](Procedure& proc, BasicBlock* block, Value* other) {
        MemoryValue* load = block->appendNew<MemoryValue>(proc, Load, Int32, Origin(), other, 0, HeapRange(30, 40), HeapRange(2000, 2001));
        appendSink(proc, block, other, load, 8);
    };
    CHECK_EQ(deadStoreSurvivors(Store, Store, fencedLoad), 2u);
}

void testReduceStrengthDeadStoreKeptAcrossLoad()
{
    if (Options::defaultB3OptLevel() < 2)
        return;

    auto loadOther = [](Procedure& proc, BasicBlock* block, Value* other) {
        appendSink(proc, block, other, appendLoadWithRange(proc, block, Load, Int64, other, HeapRange(15, 16)), 8);
    };
    CHECK_EQ(deadStoreSurvivors(Store, Store, loadOther), 2u);

    Procedure proc;
    BasicBlock* root = proc.addBlock();
    Value* pointer = root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0);
    Value* other = root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR1);
    Value* a = root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR2);
    Value* b = root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR3);
    appendStoreWithRange(proc, root, Store, a, pointer, HeapRange(10, 20));
    MemoryValue* load = appendLoadWithRange(proc, root, Load, Int64, other, HeapRange(10, 20));
    appendStoreWithRange(proc, root, Store, b, pointer, HeapRange(10, 20));
    root->clearSuccessors();
    root->appendNew<Value>(proc, Return, Origin(), load);

    runReduceStrengthInitial(proc);

    CHECK_EQ(valuesStoredThrough(proc, pointer).size(), 2u);

    int64_t storage = 0;
    CHECK_EQ(invoke<int64_t>(*compileProc(proc), &storage, &storage, static_cast<int64_t>(1), static_cast<int64_t>(2)), 1);
    CHECK_EQ(storage, 2);
}

void testReduceStrengthDeadStoreKeptAcrossCheck()
{
    if (Options::defaultB3OptLevel() < 2)
        return;

    auto check = [](Procedure& proc, BasicBlock* block, Value* other) {
        CheckValue* check = block->appendNew<CheckValue>(proc, Check, Origin(), block->appendNew<Value>(proc, Trunc, Origin(), other));
        check->setGenerator([](CCallHelpers& jit, const StackmapGenerationParams&) {
            jit.breakpoint();
        });
    };
    CHECK_EQ(deadStoreSurvivors(Store, Store, check), 2u);
}

void testReduceStrengthDeadStoreKeptWhenNarrower()
{
    Procedure proc;
    if (proc.optLevel() < 2)
        return;

    BasicBlock* root = proc.addBlock();
    Value* pointer = root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0);
    Value* a = root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR1);
    Value* b = root->appendNew<Value>(proc, Trunc, Origin(), root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR2));
    appendStoreWithRange(proc, root, Store, a, pointer, HeapRange(10, 20));
    appendStoreWithRange(proc, root, Store8, b, pointer, HeapRange(10, 20));
    root->clearSuccessors();
    root->appendNew<Value>(proc, Return, Origin());

    runReduceStrengthInitial(proc);

    CHECK_EQ(valuesStoredThrough(proc, pointer).size(), 2u);
}

void testReduceStrengthDeadStoreKeptWhenTrapping()
{
    if (Options::defaultB3OptLevel() < 2)
        return;
    CHECK_EQ(deadStoreSurvivors(trapping(Store), Store, nothingBetween), 2u);
}

void testReduceStrengthDeadStoreKeptWhenKillerTraps()
{
    if (Options::defaultB3OptLevel() < 2)
        return;
    CHECK_EQ(deadStoreSurvivors(Store, trapping(Store), nothingBetween), 2u);
}

void testReduceStrengthDeadStoreDisjointBetween()
{
    if (Options::defaultB3OptLevel() < 2)
        return;

    auto storeOther = [](Procedure& proc, BasicBlock* block, Value* other) {
        appendStoreWithRange(proc, block, Store, block->appendNew<Const32Value>(proc, Origin(), 1), other, HeapRange(30, 40));
    };
    CHECK_EQ(deadStoreSurvivors(Store, Store, storeOther), 1u);
}

// Stores a to p and branches; the taken arm runs `between` and then stores b to p with
// `killerKind`, the other arm stores c to p only if `otherArmStores`. Returns how many stores to p
// remain after reduction, then checks that the compiled code leaves p as the source says.
template<typename Between>
static size_t crossBlockDeadStoreSurvivors(Kind killerKind, bool otherArmStores, const Between& between)
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    BasicBlock* taken = proc.addBlock();
    BasicBlock* notTaken = proc.addBlock();

    Value* pointer = root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0);
    Value* other = root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR1);
    Value* a = root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR2);
    Value* b = root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR3);
    Value* c = root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR4);
    Value* condition = root->appendNew<Value>(proc, Trunc, Origin(), root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR5));
    appendStoreWithRange(proc, root, Store, a, pointer, HeapRange(10, 20));
    root->setSuccessors(FrequentedBlock(taken), FrequentedBlock(notTaken));
    root->appendNew<Value>(proc, Branch, Origin(), condition);

    between(proc, taken, other);
    appendStoreWithRange(proc, taken, killerKind, b, pointer, HeapRange(10, 20));
    taken->clearSuccessors();
    taken->appendNew<Value>(proc, Return, Origin());

    if (otherArmStores)
        appendStoreWithRange(proc, notTaken, Store, c, pointer, HeapRange(10, 20));
    notTaken->clearSuccessors();
    notTaken->appendNew<Value>(proc, Return, Origin());

    runReduceStrengthInitial(proc);
    size_t survivors = valuesStoredThrough(proc, pointer).size();

    auto code = compileProc(proc);
    int64_t storage = 0;
    int64_t otherStorage[2] = { };
    invoke<void>(*code, &storage, otherStorage, static_cast<int64_t>(1), static_cast<int64_t>(2), static_cast<int64_t>(3), static_cast<int64_t>(1));
    CHECK_EQ(storage, 2);
    storage = 0;
    invoke<void>(*code, &storage, otherStorage, static_cast<int64_t>(1), static_cast<int64_t>(2), static_cast<int64_t>(3), static_cast<int64_t>(0));
    CHECK_EQ(storage, otherArmStores ? 3 : 1);
    return survivors;
}

void testReduceStrengthDeadStoreAcrossBlocks()
{
    if (Options::defaultB3OptLevel() < 2)
        return;

    CHECK_EQ(crossBlockDeadStoreSurvivors(Store, true, nothingBetween), 2u);
}

void testReduceStrengthDeadStoreAcrossBlocksKeptOnPathToExit()
{
    if (Options::defaultB3OptLevel() < 2)
        return;

    CHECK_EQ(crossBlockDeadStoreSurvivors(Store, false, nothingBetween), 2u);
}

void testReduceStrengthDeadStoreAcrossBlocksKeptAcrossLoad()
{
    if (Options::defaultB3OptLevel() < 2)
        return;

    auto loadOther = [](Procedure& proc, BasicBlock* block, Value* other) {
        appendSink(proc, block, other, appendLoadWithRange(proc, block, Load, Int64, other, HeapRange(15, 16)), 8);
    };
    CHECK_EQ(crossBlockDeadStoreSurvivors(Store, true, loadOther), 3u);
}

void testReduceStrengthDeadStoreAcrossBlocksKeptAcrossCheck()
{
    if (Options::defaultB3OptLevel() < 2)
        return;

    auto check = [](Procedure& proc, BasicBlock* block, Value* other) {
        CheckValue* check = block->appendNew<CheckValue>(proc, Check, Origin(), block->appendNew<Value>(proc, Equal, Origin(), other, block->appendNew<Const64Value>(proc, Origin(), 0)));
        check->setGenerator([](CCallHelpers& jit, const StackmapGenerationParams&) {
            jit.breakpoint();
        });
    };
    CHECK_EQ(crossBlockDeadStoreSurvivors(Store, true, check), 3u);
}

void testReduceStrengthDeadStoreAcrossBlocksKeptWhenKillerTraps()
{
    if (Options::defaultB3OptLevel() < 2)
        return;
    CHECK_EQ(crossBlockDeadStoreSurvivors(trapping(Store), true, nothingBetween), 3u);
}

// The loop body reads p before storing to it, so the store from the previous iteration is live.
void testReduceStrengthDeadStoreAcrossBlocksKeptInLoop()
{
    if (Options::defaultB3OptLevel() < 2)
        return;

    Procedure proc;
    BasicBlock* root = proc.addBlock();
    BasicBlock* loop = proc.addBlock();
    BasicBlock* exit = proc.addBlock();

    Value* pointer = root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0);
    Value* count = root->appendNew<Value>(proc, Trunc, Origin(), root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR1));
    UpsilonValue* initial = root->appendNew<UpsilonValue>(proc, Origin(), count);
    root->setSuccessors(FrequentedBlock(loop));
    root->appendNew<Value>(proc, Jump, Origin());

    Value* remaining = loop->appendNew<Value>(proc, Phi, Int32, Origin());
    initial->setPhi(remaining);
    MemoryValue* previous = appendLoadWithRange(proc, loop, Load, Int32, pointer, HeapRange(10, 20));
    Value* next = loop->appendNew<Value>(proc, Add, Origin(), previous, loop->appendNew<Const32Value>(proc, Origin(), 1));
    appendStoreWithRange(proc, loop, Store, next, pointer, HeapRange(10, 20));
    Value* decremented = loop->appendNew<Value>(proc, Sub, Origin(), remaining, loop->appendNew<Const32Value>(proc, Origin(), 1));
    loop->appendNew<UpsilonValue>(proc, Origin(), decremented, remaining);
    loop->setSuccessors(FrequentedBlock(loop), FrequentedBlock(exit));
    loop->appendNew<Value>(proc, Branch, Origin(), decremented);

    appendStoreWithRange(proc, exit, Store, exit->appendNew<Const32Value>(proc, Origin(), 100), pointer, HeapRange(10, 20));
    exit->clearSuccessors();
    exit->appendNew<Value>(proc, Return, Origin());

    runReduceStrengthInitial(proc);

    CHECK_EQ(valuesStoredThrough(proc, pointer).size(), 2u);

    int32_t storage = 0;
    invoke<void>(*compileProc(proc), &storage, static_cast<int64_t>(3));
    CHECK_EQ(storage, 100);
}

void testStoreAfterClobberExitsSideways()
{
    Procedure proc;
    GPRReg pinnedBaseGPR = GPRInfo::argumentGPR0;
    GPRReg pinnedSizeGPR = GPRInfo::argumentGPR1;
    proc.pinRegister(pinnedBaseGPR);
    proc.pinRegister(pinnedSizeGPR);

    // Please don't make me save anything.
    RegisterSet csrs;
    csrs.merge(RegisterSet::calleeSaveRegisters());
    csrs.exclude(RegisterSet::stackRegisters());
    csrs.forEach(
        [&] (Reg reg) {
            CHECK(reg != pinnedBaseGPR);
            CHECK(reg != pinnedSizeGPR);
            proc.pinRegister(reg);
        });

    proc.setWasmBoundsCheckGenerator([=](CCallHelpers& jit, WasmBoundsCheckValue*, GPRReg pinnedGPR) {
        CHECK_EQ(pinnedGPR, pinnedSizeGPR);

        jit.move(CCallHelpers::TrustedImm32(42), GPRInfo::returnValueGPR);
        jit.emitFunctionEpilogue();
        jit.ret();
    });

    BasicBlock* root = proc.addBlock();

    Value* pointer = root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR2);
    auto* resultAddress = root->appendNew<WasmAddressValue>(proc, Origin(), pointer, pinnedBaseGPR);
    root->appendNew<MemoryValue>(proc, Store, Origin(), root->appendNew<Const32Value>(proc, Origin(), 10), resultAddress, 0);

    pointer = root->appendNew<Value>(proc, Trunc, Origin(), pointer);
    root->appendNew<WasmBoundsCheckValue>(proc, Origin(), pinnedSizeGPR, pointer, 0);

    root->appendNew<MemoryValue>(proc, Store, Origin(), root->appendNew<Const32Value>(proc, Origin(), 20), resultAddress, 0);
    root->clearSuccessors();
    root->appendNew<Value>(proc, Return, Origin(), root->appendNew<Const32Value>(proc, Origin(), 30));

    auto binary = compileProc(proc);

    uint64_t* memory = new uint64_t[10];
    uint64_t ptr = 1*8;

    for (int i = 0; i < 10; ++i)
        memory[i] = 0;

    {
        int result = invoke<int>(*binary, memory, 16, ptr);

        CHECK_EQ(result, 30);
        CHECK_EQ(memory[0], 0ul);
        CHECK_EQ(memory[1], 20ul);
        CHECK_EQ(memory[2], 0ul);
    }

    memory[1] = 0;

    {
        int result = invoke<int>(*binary, memory, 1, ptr);

        CHECK_EQ(result, 42);
        CHECK_EQ(memory[0], 0ul);
        CHECK_EQ(memory[1], 10ul);
        CHECK_EQ(memory[2], 0ul);
    }

    memory[1] = 0;

    delete[] memory;
}

void testStoreAfterClobberDifferentWidth()
{
    Procedure proc;
    GPRReg pinnedBaseGPR = GPRInfo::argumentGPR0;
    proc.pinRegister(pinnedBaseGPR);

    BasicBlock* root = proc.addBlock();

    auto* pointer = root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR1);
    auto* resultAddress = root->appendNew<WasmAddressValue>(proc, Origin(), pointer, pinnedBaseGPR);
    root->appendNew<MemoryValue>(proc, Store, Origin(), root->appendNew<ConstPtrValue>(proc, Origin(), -1), resultAddress, 0);
    root->appendNew<MemoryValue>(proc, Store, Origin(), root->appendNew<Const32Value>(proc, Origin(), 20), resultAddress, 0);
    root->clearSuccessors();
    root->appendNew<Value>(proc, Return, Origin(), root->appendNew<Const32Value>(proc, Origin(), 30));

    auto binary = compileProc(proc);

    uint64_t* memory = new uint64_t[10];
    uintptr_t ptr = 1*8;

    for (int i = 0; i < 10; ++i)
        memory[i] = 0;

    {
        int result = invoke<int>(*binary, memory, ptr);

        CHECK_EQ(result, 30);
        CHECK_EQ(memory[0], 0ul);
        CHECK_EQ(memory[1], (0xFFFFFFFF00000000ul | 20ul));
        CHECK_EQ(memory[2], 0ul);
    }

    delete[] memory;
}

void testStoreAfterClobberDifferentWidthSuccessor()
{
    Procedure proc;
    GPRReg pinnedBaseGPR = GPRInfo::argumentGPR0;
    proc.pinRegister(pinnedBaseGPR);

    BasicBlock* root = proc.addBlock();
    BasicBlock* a = proc.addBlock();
    BasicBlock* b = proc.addBlock();
    BasicBlock* c = proc.addBlock();
    BasicBlock* continuation = proc.addBlock();

    auto* pointer = root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR1);
    auto* path = root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR2);
    auto* resultAddress = root->appendNew<WasmAddressValue>(proc, Origin(), pointer, pinnedBaseGPR);
    root->appendNew<MemoryValue>(proc, Store, Origin(), root->appendNew<ConstPtrValue>(proc, Origin(), -1), resultAddress, 0);

    SwitchValue* switchValue = root->appendNew<SwitchValue>(proc, Origin(), path);
    switchValue->setFallThrough(FrequentedBlock(c));
    switchValue->appendCase(SwitchCase(0, FrequentedBlock(a)));
    switchValue->appendCase(SwitchCase(1, FrequentedBlock(b)));

    a->appendNew<MemoryValue>(proc, Store, Origin(), a->appendNew<Const32Value>(proc, Origin(), 10), resultAddress, 0);
    b->appendNew<MemoryValue>(proc, Store, Origin(), b->appendNew<Const32Value>(proc, Origin(), 20), resultAddress, 0);
    c->appendNew<MemoryValue>(proc, Store, Origin(), c->appendNew<Const32Value>(proc, Origin(), 30), resultAddress, 0);

    for (auto* i : { a, b, c }) {
        i->setSuccessors(FrequentedBlock(continuation));
        i->appendNew<Value>(proc, Jump, Origin());
        i->setSuccessors(FrequentedBlock(continuation));
    }

    continuation->clearSuccessors();
    continuation->appendNew<Value>(proc, Return, Origin(), continuation->appendNew<Const32Value>(proc, Origin(), 40));

    auto binary = compileProc(proc);

    uint64_t* memory = new uint64_t[10];
    uintptr_t ptr = 1*8;

    for (int i = 0; i < 10; ++i)
        memory[i] = 0;

    {
        int result = invoke<int>(*binary, memory, ptr, 0);

        CHECK_EQ(result, 40);
        CHECK_EQ(memory[0], 0ul);
        CHECK_EQ(memory[1], (0xFFFFFFFF00000000ul | 10ul));
        CHECK_EQ(memory[2], 0ul);
    }

    memory[1] = 0;

    {
        int result = invoke<int>(*binary, memory, ptr, 1);

        CHECK_EQ(result, 40);
        CHECK_EQ(memory[0], 0ul);
        CHECK_EQ(memory[1], (0xFFFFFFFF00000000ul | 20ul));
        CHECK_EQ(memory[2], 0ul);
    }

    memory[1] = 0;

    {
        int result = invoke<int>(*binary, memory, ptr, 2);

        CHECK_EQ(result, 40);
        CHECK_EQ(memory[0], 0ul);
        CHECK_EQ(memory[1], (0xFFFFFFFF00000000ul | 30ul));
        CHECK_EQ(memory[2], 0ul);
    }

    delete[] memory;
}

void testStoreAfterClobberExitsSidewaysSuccessor()
{
    Procedure proc;
    GPRReg pinnedBaseGPR = GPRInfo::argumentGPR0;
    GPRReg pinnedSizeGPR = GPRInfo::argumentGPR1;
    proc.pinRegister(pinnedBaseGPR);
    proc.pinRegister(pinnedSizeGPR);

    // Please don't make me save anything.
    RegisterSet csrs;
    csrs.merge(RegisterSet::calleeSaveRegisters());
    csrs.exclude(RegisterSet::stackRegisters());
    csrs.forEach(
        [&] (Reg reg) {
            CHECK(reg != pinnedBaseGPR);
            CHECK(reg != pinnedSizeGPR);
            proc.pinRegister(reg);
        });

    proc.setWasmBoundsCheckGenerator([=](CCallHelpers& jit, WasmBoundsCheckValue*, GPRReg pinnedGPR) {
        CHECK_EQ(pinnedGPR, pinnedSizeGPR);

        jit.move(CCallHelpers::TrustedImm32(42), GPRInfo::returnValueGPR);
        jit.emitFunctionEpilogue();
        jit.ret();
    });

    BasicBlock* root = proc.addBlock();
    BasicBlock* a = proc.addBlock();
    BasicBlock* b = proc.addBlock();
    BasicBlock* c = proc.addBlock();
    BasicBlock* continuation = proc.addBlock();

    Value* pointer = root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR2);
    auto* path = root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR3);
    auto* resultAddress = root->appendNew<WasmAddressValue>(proc, Origin(), pointer, pinnedBaseGPR);
    root->appendNew<MemoryValue>(proc, Store, Origin(), root->appendNew<ConstPtrValue>(proc, Origin(), -1), resultAddress, 0);

    SwitchValue* switchValue = root->appendNew<SwitchValue>(proc, Origin(), path);
    switchValue->setFallThrough(FrequentedBlock(c));
    switchValue->appendCase(SwitchCase(0, FrequentedBlock(a)));
    switchValue->appendCase(SwitchCase(1, FrequentedBlock(b)));

    pointer = b->appendNew<Value>(proc, Trunc, Origin(), pointer);
    b->appendNew<WasmBoundsCheckValue>(proc, Origin(), pinnedSizeGPR, pointer, 0);

    UpsilonValue* takeA = a->appendNew<UpsilonValue>(proc, Origin(), a->appendNew<Const64Value>(proc, Origin(), 10));
    UpsilonValue* takeB = b->appendNew<UpsilonValue>(proc, Origin(), b->appendNew<Const64Value>(proc, Origin(), 20));
    UpsilonValue* takeC = c->appendNew<UpsilonValue>(proc, Origin(), c->appendNew<Const64Value>(proc, Origin(), 30));

    for (auto* i : { a, b, c }) {
        i->setSuccessors(FrequentedBlock(continuation));
        i->appendNew<Value>(proc, Jump, Origin());
        i->setSuccessors(FrequentedBlock(continuation));
    }

    auto* takenPhi = continuation->appendNew<Value>(proc, Phi, Int64, Origin());
    continuation->appendNew<MemoryValue>(proc, Store, Origin(), takenPhi, resultAddress, 0);
    continuation->clearSuccessors();
    continuation->appendNew<Value>(proc, Return, Origin(), continuation->appendNew<Const32Value>(proc, Origin(), 40));

    takeA->setPhi(takenPhi);
    takeB->setPhi(takenPhi);
    takeC->setPhi(takenPhi);

    auto binary = compileProc(proc);

    uint64_t* memory = new uint64_t[10];
    uintptr_t ptr = 1*8;

    for (int i = 0; i < 10; ++i)
        memory[i] = 0;

    {
        int result = invoke<int>(*binary, memory, 16, ptr, 0);

        CHECK_EQ(result, 40);
        CHECK_EQ(memory[0], 0ul);
        CHECK_EQ(memory[1], 10ul);
        CHECK_EQ(memory[2], 0ul);
    }

    memory[1] = 0;

    {
        int result = invoke<int>(*binary, memory, 16, ptr, 1);

        CHECK_EQ(result, 40);
        CHECK_EQ(memory[0], 0ul);
        CHECK_EQ(memory[1], 20ul);
        CHECK_EQ(memory[2], 0ul);
    }

    memory[1] = 0;

    {
        int result = invoke<int>(*binary, memory, 16, ptr, 2);

        CHECK_EQ(result, 40);
        CHECK_EQ(memory[0], 0ul);
        CHECK_EQ(memory[1], 30ul);
        CHECK_EQ(memory[2], 0ul);
    }

    memory[1] = 0;

    {
        int result = invoke<int>(*binary, memory, 1, ptr, 2);

        CHECK_EQ(result, 40);
        CHECK_EQ(memory[0], 0ul);
        CHECK_EQ(memory[1], 30ul);
        CHECK_EQ(memory[2], 0ul);
    }

    memory[1] = 0;

    {
        int result = invoke<int>(*binary, memory, 1, ptr, 1);

        CHECK_EQ(result, 42);
        CHECK_EQ(memory[0], 0ul);
        CHECK_EQ(memory[1], (0xFFFFFFFFFFFFFFFFul));
        CHECK_EQ(memory[2], 0ul);
    }

    delete[] memory;
}

void testNarrowLoad()
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    auto* value1 = root->appendNew<MemoryValue>(proc, Load, Int64, Origin(), root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0));
    auto* value2 = root->appendNew<MemoryValue>(proc, Load, Int32, Origin(), root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0));
    root->clearSuccessors();
    root->appendNew<Value>(proc, Return, Origin(), root->appendNew<Value>(proc, Add, Int64, Origin(), value1, root->appendNew<Value>(proc, ZExt32, Int64, Origin(), value2)));

    uint64_t value = 0x1000000010000000ULL;
    CHECK_EQ(compileAndRun<uint64_t>(proc, &value), 0x1000000020000000ULL);
}

void testNarrowLoadClobber()
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    auto* address = root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0);
    auto* value1 = root->appendNew<MemoryValue>(proc, Load, Int64, Origin(), address);
    root->appendNew<MemoryValue>(proc, Store, Origin(), root->appendNew<Const64Value>(proc, Origin(), 0), address, 0);
    auto* value2 = root->appendNew<MemoryValue>(proc, Load, Int32, Origin(), root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0));
    root->clearSuccessors();
    root->appendNew<Value>(proc, Return, Origin(), root->appendNew<Value>(proc, Add, Int64, Origin(), value1, root->appendNew<Value>(proc, ZExt32, Int64, Origin(), value2)));

    uint64_t value = 0x1000000010000000ULL;
    CHECK_EQ(compileAndRun<uint64_t>(proc, &value), 0x1000000010000000ULL);
    CHECK_EQ(value, 0x0000000000000000ULL);
}

void testNarrowLoadClobberNarrow()
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    auto* address = root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0);
    auto* value1 = root->appendNew<MemoryValue>(proc, Load, Int64, Origin(), address);
    root->appendNew<MemoryValue>(proc, Store, Origin(), root->appendNew<Const32Value>(proc, Origin(), 0), address, 0);
    auto* value2 = root->appendNew<MemoryValue>(proc, Load, Int32, Origin(), root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0));
    root->clearSuccessors();
    root->appendNew<Value>(proc, Return, Origin(), root->appendNew<Value>(proc, Add, Int64, Origin(), value1, root->appendNew<Value>(proc, ZExt32, Int64, Origin(), value2)));

    uint64_t value = 0x1000000010000000ULL;
    CHECK_EQ(compileAndRun<uint64_t>(proc, &value), 0x1000000010000000ULL);
    CHECK_EQ(value, 0x1000000000000000ULL);
}

void testNarrowLoadNotClobber()
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    auto* address = root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0);
    auto* value1 = root->appendNew<MemoryValue>(proc, Load, Int64, Origin(), address);
    root->appendNew<MemoryValue>(proc, Store, Origin(), root->appendNew<Const32Value>(proc, Origin(), 0), address, 4);
    auto* value2 = root->appendNew<MemoryValue>(proc, Load, Int32, Origin(), root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0));
    root->clearSuccessors();
    root->appendNew<Value>(proc, Return, Origin(), root->appendNew<Value>(proc, Add, Int64, Origin(), value1, root->appendNew<Value>(proc, ZExt32, Int64, Origin(), value2)));

    uint64_t value = 0x1000000010000000ULL;
    CHECK_EQ(compileAndRun<uint64_t>(proc, &value), 0x1000000020000000ULL);
    CHECK_EQ(value, 0x0000000010000000ULL);
}

void testNarrowLoadUpper()
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    auto* address = root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0);
    auto* value1 = root->appendNew<MemoryValue>(proc, Load, Int64, Origin(), address);
    auto* value2 = root->appendNew<MemoryValue>(proc, Load, Int32, Origin(), address, 4);
    root->clearSuccessors();
    root->appendNew<Value>(proc, Return, Origin(), root->appendNew<Value>(proc, Add, Int64, Origin(), value1, root->appendNew<Value>(proc, ZExt32, Int64, Origin(), value2)));

    uint64_t value = 0x2000000010000000ULL;
    CHECK_EQ(compileAndRun<uint64_t>(proc, &value), 0x2000000030000000ULL);
}

void testConstDoubleMove()
{
    // FMOV
    {
        auto encode = [](uint64_t value) -> double {
            constexpr unsigned E = 11;
            constexpr unsigned F = 64 - E - 1;
            uint64_t sign = (value & 0b10000000U) ? 1 : 0;
            uint64_t upper = (value & 0b01000000U) ? 0b01111111100U : 0b10000000000U;
            uint64_t exp = upper | ((value & 0b00110000U) >> 4);
            uint64_t frac = (value & 0b1111U) << (F - 4);
            return std::bit_cast<double>((sign << 63) | (exp << F) | frac);
        };

        for (uint8_t i = 0; i < UINT8_MAX; ++i) {
            Procedure proc;
            BasicBlock* root = proc.addBlock();
            root->clearSuccessors();
            root->appendNew<Value>(proc, Return, Origin(), root->appendNew<ConstDoubleValue>(proc, Origin(), encode(i)));
            CHECK_EQ(compileAndRun<double>(proc), encode(i));
        }
    }

    // MOVI
    {
        auto encode = [](uint64_t value) -> uint64_t {
            auto bits = [](bool flag) -> uint64_t {
                return (flag) ? 0b11111111ULL : 0b00000000ULL;
            };

            return (bits(value & (1U << 7)) << 56)
                | (bits(value & (1U << 6)) << 48)
                | (bits(value & (1U << 5)) << 40)
                | (bits(value & (1U << 4)) << 32)
                | (bits(value & (1U << 3)) << 24)
                | (bits(value & (1U << 2)) << 16)
                | (bits(value & (1U << 1)) << 8)
                | (bits(value & (1U << 0)) << 0);
        };

        for (uint8_t i = 0; i < UINT8_MAX; ++i) {
            Procedure proc;
            BasicBlock* root = proc.addBlock();
            root->clearSuccessors();
            root->appendNew<Value>(proc, Return, Origin(), root->appendNew<ConstDoubleValue>(proc, Origin(), std::bit_cast<double>(encode(i))));
            CHECK_EQ(std::bit_cast<uint64_t>(compileAndRun<double>(proc)), encode(i));
        }
    }
}

void testConstFloatMove()
{
    // FMOV
    auto encode = [](uint64_t value) -> float {
        constexpr unsigned E = 8;
        constexpr unsigned F = 32 - E - 1;
        uint32_t sign = (value & 0b10000000U) ? 1 : 0;
        uint32_t upper = (value & 0b01000000U) ? 0b01111100U : 0b10000000U;
        uint32_t exp = upper | ((value & 0b00110000U) >> 4);
        uint32_t frac = (value & 0b1111U) << (F - 4);
        return std::bit_cast<float>((sign << 31) | (exp << F) | frac);
    };

    for (uint8_t i = 0; i < UINT8_MAX; ++i) {
        Procedure proc;
        BasicBlock* root = proc.addBlock();
        root->clearSuccessors();
        root->appendNew<Value>(proc, Return, Origin(), root->appendNew<ConstFloatValue>(proc, Origin(), encode(i)));
        CHECK_EQ(compileAndRun<float>(proc), encode(i));
    }
}

void testSShrCompare32(int32_t constantValue)
{
    auto compile = [&](B3::Opcode opcode, uint32_t shiftAmount, uint32_t constantValue) {
        Procedure proc;
        BasicBlock* root = proc.addBlock();
        auto arguments = cCallArgumentValues<int32_t>(proc, root);
        auto* shifted = root->appendNew<Value>(proc, SShr, Origin(), arguments[0], root->appendNew<Const32Value>(proc, Origin(), shiftAmount));
        auto* constant = root->appendNew<Const32Value>(proc, Origin(), constantValue);
        auto* comparison = root->appendNew<Value>(proc, opcode, Origin(), shifted, constant);
        root->clearSuccessors();
        root->appendNew<Value>(proc, Return, Origin(), comparison);
        return compileProc(proc);
    };

    auto testWithOpcode = [&](B3::Opcode opcode, auto compare) {
        for (uint32_t shiftAmount = 0; shiftAmount < 32; ++shiftAmount) {
            auto code = compile(opcode, shiftAmount, constantValue);
            for (auto input : int32OperandsMore()) {
                for (uint32_t step = 0; step < 1000; ++step) {
                    int32_t before = static_cast<uint32_t>(input.value) - step;
                    int32_t middle = static_cast<uint32_t>(input.value);
                    int32_t after = static_cast<uint32_t>(input.value) + step;
                    CHECK_EQ(invoke<bool>(*code, before), compare(shiftAmount, constantValue, before));
                    CHECK_EQ(invoke<bool>(*code, middle), compare(shiftAmount, constantValue, middle));
                    CHECK_EQ(invoke<bool>(*code, after), compare(shiftAmount, constantValue, after));
                }
            }
        }
    };

    testWithOpcode(Above, [](uint32_t shiftAmount, uint32_t constantValue, int32_t value) { return static_cast<uint32_t>(value >> shiftAmount) > constantValue; });
    testWithOpcode(AboveEqual, [](uint32_t shiftAmount, uint32_t constantValue, int32_t value) { return static_cast<uint32_t>(value >> shiftAmount) >= constantValue; });
    testWithOpcode(Below, [](uint32_t shiftAmount, uint32_t constantValue, int32_t value) { return static_cast<uint32_t>(value >> shiftAmount) < constantValue; });
    testWithOpcode(BelowEqual, [](uint32_t shiftAmount, uint32_t constantValue, int32_t value) { return static_cast<uint32_t>(value >> shiftAmount) <= constantValue; });
}

void testSShrCompare64(int64_t constantValue)
{
    auto compile = [&](B3::Opcode opcode, uint64_t shiftAmount, uint64_t constantValue) {
        Procedure proc;
        BasicBlock* root = proc.addBlock();
        auto arguments = cCallArgumentValues<uint64_t>(proc, root);
        auto* shifted = root->appendNew<Value>(proc, SShr, Origin(), arguments[0], root->appendNew<Const32Value>(proc, Origin(), shiftAmount));
        auto* constant = root->appendNew<Const64Value>(proc, Origin(), constantValue);
        auto* comparison = root->appendNew<Value>(proc, opcode, Origin(), shifted, constant);
        root->clearSuccessors();
        root->appendNew<Value>(proc, Return, Origin(), comparison);
        return compileProc(proc);
    };

    auto testWithOpcode = [&](B3::Opcode opcode, auto compare) {
        for (uint64_t shiftAmount = 0; shiftAmount < 64; ++shiftAmount) {
            auto code = compile(opcode, shiftAmount, constantValue);
            for (auto input : int64OperandsMore()) {
                for (uint64_t step = 0; step < 1000; ++step) {
                    int64_t before = static_cast<uint64_t>(input.value) - step;
                    int64_t middle = static_cast<uint64_t>(input.value);
                    int64_t after = static_cast<uint64_t>(input.value) + step;
                    CHECK_EQ(invoke<bool>(*code, before), compare(shiftAmount, constantValue, before));
                    CHECK_EQ(invoke<bool>(*code, middle), compare(shiftAmount, constantValue, middle));
                    CHECK_EQ(invoke<bool>(*code, after), compare(shiftAmount, constantValue, after));
                }
            }
        }
    };

    testWithOpcode(Above, [](uint64_t shiftAmount, uint64_t constantValue, int64_t value) { return static_cast<uint64_t>(value >> shiftAmount) > constantValue; });
    testWithOpcode(AboveEqual, [](uint64_t shiftAmount, uint64_t constantValue, int64_t value) { return static_cast<uint64_t>(value >> shiftAmount) >= constantValue; });
    testWithOpcode(Below, [](uint64_t shiftAmount, uint64_t constantValue, int64_t value) { return static_cast<uint64_t>(value >> shiftAmount) < constantValue; });
    testWithOpcode(BelowEqual, [](uint64_t shiftAmount, uint64_t constantValue, int64_t value) { return static_cast<uint64_t>(value >> shiftAmount) <= constantValue; });
}

void testMulHigh64()
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    auto arguments = cCallArgumentValues<int64_t, int64_t>(proc, root);

    Value* argumentA = arguments[0];
    Value* argumentB = arguments[1];

    root->clearSuccessors();
    root->appendNew<Value>(proc, Return, Origin(), root->appendNew<Value>(
            proc, MulHigh, Origin(),
            argumentA,
            argumentB));

    auto code = compileProc(proc);
    for (auto a : int64Operands()) {
        for (auto b : int64Operands())
            CHECK_EQ(invoke<int64_t>(*code, a.value, b.value), static_cast<int64_t>((static_cast<Int128>(a.value) * static_cast<Int128>(b.value)) >> 64));
    }
}

void testMulHigh32()
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    auto arguments = cCallArgumentValues<int32_t, int32_t>(proc, root);

    root->clearSuccessors();
    root->appendNew<Value>(proc, Return, Origin(), root->appendNew<Value>(
            proc, MulHigh, Origin(),
            arguments[0],
            arguments[1]));

    auto code = compileProc(proc);
    for (auto a : int32Operands()) {
        for (auto b : int32Operands())
            CHECK_EQ(invoke<int32_t>(*code, a.value, b.value), static_cast<int32_t>((static_cast<int64_t>(a.value) * static_cast<int64_t>(b.value)) >> 32));
    }
}

void testUMulHigh64()
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    auto arguments = cCallArgumentValues<uint64_t, uint64_t>(proc, root);

    Value* argumentA = arguments[0];
    Value* argumentB = arguments[1];

    root->clearSuccessors();
    root->appendNew<Value>(proc, Return, Origin(), root->appendNew<Value>(
            proc, UMulHigh, Origin(),
            argumentA,
            argumentB));

    auto code = compileProc(proc);
    for (auto a : int64Operands()) {
        for (auto b : int64Operands())
            CHECK_EQ(invoke<uint64_t>(*code, a.value, b.value), static_cast<uint64_t>((static_cast<UInt128>(static_cast<uint64_t>(a.value)) * static_cast<UInt128>(static_cast<uint64_t>(b.value))) >> 64));
    }
}

void testUMulHigh32()
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    auto arguments = cCallArgumentValues<uint32_t, uint32_t>(proc, root);

    root->clearSuccessors();
    root->appendNew<Value>(proc, Return, Origin(), root->appendNew<Value>(
            proc, UMulHigh, Origin(),
            arguments[0],
            arguments[1]));

    auto code = compileProc(proc);
    for (auto a : int32Operands()) {
        for (auto b : int32Operands())
            CHECK_EQ(invoke<uint32_t>(*code, a.value, b.value), static_cast<uint32_t>((static_cast<uint64_t>(static_cast<uint32_t>(a.value)) * static_cast<uint64_t>(static_cast<uint32_t>(b.value))) >> 32));
    }
}

void testMemoryCopy()
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    auto arguments = cCallArgumentValues<void*, void*, void*>(proc, root);
    root->appendNew<BulkMemoryValue>(proc, MemoryCopy, Origin(), arguments[0], arguments[1], arguments[2]);
    root->clearSuccessors();
    root->appendNew<Value>(proc, Return, Origin());

    auto code = compileProc(proc);
    Vector<uint8_t> src(4096 + 1024);
    Vector<uint8_t> dst(4096 + 1024);

    for (unsigned base = 1; base < 4096; base <<= 1) {
        unsigned offset = 0;
        for (auto a : int32Operands()) {
            dst.fill(0);
            src.fill(static_cast<uint8_t>(a.value));
            invoke<void>(*code, dst.mutableSpan().data(), src.span().data(), static_cast<uintptr_t>(base + offset));
            for (unsigned i = 0; i < (base + offset); ++i)
                CHECK_EQ(dst[i], static_cast<uint8_t>(a.value));
            CHECK_EQ(dst[(base + offset)], 0);
            ++offset;
        }
    }

    for (unsigned base = 1; base < 4096; base <<= 1) {
        for (unsigned i = 0; i < src.size(); ++i)
            src[i] = i;
        invoke<void>(*code, src.mutableSpan().data(), src.span().data() + 1, static_cast<uintptr_t>(base));
        for (unsigned i = 0; i < base; ++i)
            CHECK_EQ(src[i], static_cast<uint8_t>(i + 1));
        CHECK_EQ(src[base], static_cast<uint8_t>(base));
    }

    for (unsigned base = 1; base < 4096; base <<= 1) {
        for (unsigned i = 0; i < src.size(); ++i)
            src[i] = i;
        invoke<void>(*code, src.mutableSpan().data() + 1, src.span().data(), static_cast<uintptr_t>(base));
        for (unsigned i = 0; i < base; ++i)
            CHECK_EQ(src[i + 1], static_cast<uint8_t>(i));
        CHECK_EQ(src[0], 0);
    }
}

void testMemoryCopyConstant()
{
    Vector<uint8_t> src(4096 + 1024);
    Vector<uint8_t> dst(4096 + 1024);

    for (unsigned width = 0; width < 128; ++width) {
        Procedure proc;
        BasicBlock* root = proc.addBlock();
        auto arguments = cCallArgumentValues<void*, void*>(proc, root);
        root->appendNew<BulkMemoryValue>(proc, MemoryCopy, Origin(), arguments[0], arguments[1], root->appendIntConstant(proc, Origin(), pointerType(), width));
        root->clearSuccessors();
        root->appendNew<Value>(proc, Return, Origin());
        auto code = compileProc(proc);

        for (auto a : int32Operands()) {
            dst.fill(0);
            src.fill(static_cast<uint8_t>(a.value));
            invoke<void>(*code, dst.mutableSpan().data(), src.span().data());
            for (unsigned i = 0; i < width; ++i)
                CHECK_EQ(dst[i], static_cast<uint8_t>(a.value));
            CHECK_EQ(dst[width], 0);
        }

        for (unsigned i = 0; i < src.size(); ++i)
            src[i] = i;
        invoke<void>(*code, src.mutableSpan().data(), src.span().data() + 1);
        for (unsigned i = 0; i < width; ++i)
            CHECK_EQ(src[i], static_cast<uint8_t>(i + 1));
        CHECK_EQ(src[width], static_cast<uint8_t>(width));

        for (unsigned i = 0; i < src.size(); ++i)
            src[i] = i;
        invoke<void>(*code, src.mutableSpan().data() + 1, src.span().data());
        for (unsigned i = 0; i < width; ++i)
            CHECK_EQ(src[i + 1], static_cast<uint8_t>(i));
        CHECK_EQ(src[0], 0);
    }
}

void testMemoryFill()
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    auto arguments = cCallArgumentValues<void*, uint32_t, void*>(proc, root);
    root->appendNew<BulkMemoryValue>(proc, MemoryFill, Origin(), arguments[0], arguments[1], arguments[2]);
    root->clearSuccessors();
    root->appendNew<Value>(proc, Return, Origin());

    auto code = compileProc(proc);
    Vector<uint8_t> src(4096 + 1024);

    for (unsigned base = 1; base < 4096; base <<= 1) {
        unsigned offset = 0;
        for (auto a : int32Operands()) {
            src.fill(0);
            invoke<void>(*code, src.mutableSpan().data(), static_cast<uint8_t>(a.value), static_cast<uintptr_t>(base + offset));
            for (unsigned i = 0; i < (base + offset); ++i)
                CHECK_EQ(src[i], static_cast<uint8_t>(a.value));
            CHECK_EQ(src[(base + offset)], 0);
            ++offset;
        }
    }
}

void testMemoryFillConstant()
{
    Vector<uint8_t> src(4096 + 1024);

    for (unsigned width = 0; width < 128; ++width) {
        for (auto a : int32Operands()) {
            Procedure proc;
            BasicBlock* root = proc.addBlock();
            auto arguments = cCallArgumentValues<void*>(proc, root);
            root->appendNew<BulkMemoryValue>(proc, MemoryFill, Origin(), arguments[0], root->appendIntConstant(proc, Origin(), Int32, a.value), root->appendIntConstant(proc, Origin(), pointerType(), width));
            root->clearSuccessors();
            root->appendNew<Value>(proc, Return, Origin());
            auto code = compileProc(proc);

            src.fill(0);
            invoke<void>(*code, src.mutableSpan().data(), static_cast<uint8_t>(a.value));
            for (unsigned i = 0; i < width; ++i)
                CHECK_EQ(src[i], static_cast<uint8_t>(a.value));
            CHECK_EQ(src[width], 0);
        }
    }
}

void testLoadImmutable()
{
    Vector<uint64_t> memory(4);
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    auto arguments = cCallArgumentValues<void*, void*>(proc, root);

    auto* value1 = root->appendNew<MemoryValue>(proc, Load, Int64, Origin(), arguments[0]);
    value1->setReadsMutability(B3::Mutability::Immutable);
    root->appendNew<MemoryValue>(proc, Store, Origin(), root->appendNew<Const32Value>(proc, Origin(), 0), arguments[1]);
    auto* value2 = root->appendNew<MemoryValue>(proc, Load, Int64, Origin(), arguments[0]);
    value2->setReadsMutability(B3::Mutability::Immutable);
    root->clearSuccessors();
    root->appendNew<Value>(proc, Return, Origin(), root->appendNew<Value>(proc, Add, Origin(), value1, value2));
    auto code = compileProc(proc);

    memory.fill(42);
    CHECK_EQ(invoke<uint64_t>(*code, memory.mutableSpan().data(), memory.mutableSpan().data() + 1), 84U);
}

// ARM64 conditional compare (ccmp) tests
// These tests verify that BitAnd/BitOr of comparisons are optimized using ccmp instruction

void testCCmpAnd32(int32_t a, int32_t b, int32_t c, int32_t d)
{
    // Test: (a == b) && (c == d)
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    BasicBlock* thenCase = proc.addBlock();
    BasicBlock* elseCase = proc.addBlock();
    auto arguments = cCallArgumentValues<int32_t, int32_t, int32_t, int32_t>(proc, root);

    Value* cmp1 = root->appendNew<Value>(proc, Equal, Origin(), arguments[0], arguments[1]);
    Value* cmp2 = root->appendNew<Value>(proc, Equal, Origin(), arguments[2], arguments[3]);
    Value* condition = root->appendNew<Value>(proc, BitAnd, Origin(), cmp1, cmp2);

    root->setSuccessors(FrequentedBlock(thenCase), FrequentedBlock(elseCase));
    root->appendNew<Value>(proc, Branch, Origin(), condition);

    thenCase->clearSuccessors();
    thenCase->appendNew<Value>(proc, Return, Origin(), thenCase->appendNew<Const32Value>(proc, Origin(), 1));

    elseCase->clearSuccessors();
    elseCase->appendNew<Value>(proc, Return, Origin(), elseCase->appendNew<Const32Value>(proc, Origin(), 0));

    int32_t expected = (a == b && c == d) ? 1 : 0;
    CHECK_EQ(compileAndRun<int32_t>(proc, a, b, c, d), expected);
}

void testCCmpAnd64(int64_t a, int64_t b, int64_t c, int64_t d)
{
    // Test: (a == b) && (c == d)
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    BasicBlock* thenCase = proc.addBlock();
    BasicBlock* elseCase = proc.addBlock();
    auto arguments = cCallArgumentValues<int64_t, int64_t, int64_t, int64_t>(proc, root);

    Value* cmp1 = root->appendNew<Value>(proc, Equal, Origin(), arguments[0], arguments[1]);
    Value* cmp2 = root->appendNew<Value>(proc, Equal, Origin(), arguments[2], arguments[3]);
    Value* condition = root->appendNew<Value>(proc, BitAnd, Origin(), cmp1, cmp2);

    root->setSuccessors(FrequentedBlock(thenCase), FrequentedBlock(elseCase));
    root->appendNew<Value>(proc, Branch, Origin(), condition);

    thenCase->clearSuccessors();
    thenCase->appendNew<Value>(proc, Return, Origin(), thenCase->appendNew<Const32Value>(proc, Origin(), 1));

    elseCase->clearSuccessors();
    elseCase->appendNew<Value>(proc, Return, Origin(), elseCase->appendNew<Const32Value>(proc, Origin(), 0));

    int32_t expected = (a == b && c == d) ? 1 : 0;
    CHECK_EQ(compileAndRun<int32_t>(proc, a, b, c, d), expected);
}

void testCCmpOr32(int32_t a, int32_t b, int32_t c, int32_t d)
{
    // Test: (a == b) || (c == d)
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    BasicBlock* thenCase = proc.addBlock();
    BasicBlock* elseCase = proc.addBlock();
    auto arguments = cCallArgumentValues<int32_t, int32_t, int32_t, int32_t>(proc, root);

    Value* cmp1 = root->appendNew<Value>(proc, Equal, Origin(), arguments[0], arguments[1]);
    Value* cmp2 = root->appendNew<Value>(proc, Equal, Origin(), arguments[2], arguments[3]);
    Value* condition = root->appendNew<Value>(proc, BitOr, Origin(), cmp1, cmp2);

    root->setSuccessors(FrequentedBlock(thenCase), FrequentedBlock(elseCase));
    root->appendNew<Value>(proc, Branch, Origin(), condition);

    thenCase->clearSuccessors();
    thenCase->appendNew<Value>(proc, Return, Origin(), thenCase->appendNew<Const32Value>(proc, Origin(), 1));

    elseCase->clearSuccessors();
    elseCase->appendNew<Value>(proc, Return, Origin(), elseCase->appendNew<Const32Value>(proc, Origin(), 0));

    int32_t expected = (a == b || c == d) ? 1 : 0;
    CHECK_EQ(compileAndRun<int32_t>(proc, a, b, c, d), expected);
}

void testCCmpOr64(int64_t a, int64_t b, int64_t c, int64_t d)
{
    // Test: (a == b) || (c == d)
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    BasicBlock* thenCase = proc.addBlock();
    BasicBlock* elseCase = proc.addBlock();
    auto arguments = cCallArgumentValues<int64_t, int64_t, int64_t, int64_t>(proc, root);

    Value* cmp1 = root->appendNew<Value>(proc, Equal, Origin(), arguments[0], arguments[1]);
    Value* cmp2 = root->appendNew<Value>(proc, Equal, Origin(), arguments[2], arguments[3]);
    Value* condition = root->appendNew<Value>(proc, BitOr, Origin(), cmp1, cmp2);

    root->setSuccessors(FrequentedBlock(thenCase), FrequentedBlock(elseCase));
    root->appendNew<Value>(proc, Branch, Origin(), condition);

    thenCase->clearSuccessors();
    thenCase->appendNew<Value>(proc, Return, Origin(), thenCase->appendNew<Const32Value>(proc, Origin(), 1));

    elseCase->clearSuccessors();
    elseCase->appendNew<Value>(proc, Return, Origin(), elseCase->appendNew<Const32Value>(proc, Origin(), 0));

    int32_t expected = (a == b || c == d) ? 1 : 0;
    CHECK_EQ(compileAndRun<int32_t>(proc, a, b, c, d), expected);
}

// 3-comparison chain tests (nested patterns)
void testCCmpAndAnd32(int32_t a, int32_t b, int32_t c, int32_t d, int32_t e, int32_t f)
{
    // Test: ((a == b) && (c == d)) && (e == f)
    // This should emit: cmp a,b; ccmp c,d; ccmp e,f; branch
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    BasicBlock* thenCase = proc.addBlock();
    BasicBlock* elseCase = proc.addBlock();
    auto arguments = cCallArgumentValues<int32_t, int32_t, int32_t, int32_t, int32_t, int32_t>(proc, root);

    Value* cmp1 = root->appendNew<Value>(proc, Equal, Origin(), arguments[0], arguments[1]);
    Value* cmp2 = root->appendNew<Value>(proc, Equal, Origin(), arguments[2], arguments[3]);
    Value* and1 = root->appendNew<Value>(proc, BitAnd, Origin(), cmp1, cmp2);
    Value* cmp3 = root->appendNew<Value>(proc, Equal, Origin(), arguments[4], arguments[5]);
    Value* condition = root->appendNew<Value>(proc, BitAnd, Origin(), and1, cmp3);

    root->setSuccessors(FrequentedBlock(thenCase), FrequentedBlock(elseCase));
    root->appendNew<Value>(proc, Branch, Origin(), condition);

    thenCase->clearSuccessors();
    thenCase->appendNew<Value>(proc, Return, Origin(), thenCase->appendNew<Const32Value>(proc, Origin(), 1));

    elseCase->clearSuccessors();
    elseCase->appendNew<Value>(proc, Return, Origin(), elseCase->appendNew<Const32Value>(proc, Origin(), 0));

    int32_t expected = (a == b && c == d && e == f) ? 1 : 0;
    CHECK_EQ(compileAndRun<int32_t>(proc, a, b, c, d, e, f), expected);
}

void testCCmpOrOr32(int32_t a, int32_t b, int32_t c, int32_t d, int32_t e, int32_t f)
{
    // Test: ((a == b) || (c == d)) || (e == f)
    // This should emit: cmp a,b; ccmp c,d; ccmp e,f; branch
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    BasicBlock* thenCase = proc.addBlock();
    BasicBlock* elseCase = proc.addBlock();
    auto arguments = cCallArgumentValues<int32_t, int32_t, int32_t, int32_t, int32_t, int32_t>(proc, root);

    Value* cmp1 = root->appendNew<Value>(proc, Equal, Origin(), arguments[0], arguments[1]);
    Value* cmp2 = root->appendNew<Value>(proc, Equal, Origin(), arguments[2], arguments[3]);
    Value* or1 = root->appendNew<Value>(proc, BitOr, Origin(), cmp1, cmp2);
    Value* cmp3 = root->appendNew<Value>(proc, Equal, Origin(), arguments[4], arguments[5]);
    Value* condition = root->appendNew<Value>(proc, BitOr, Origin(), or1, cmp3);

    root->setSuccessors(FrequentedBlock(thenCase), FrequentedBlock(elseCase));
    root->appendNew<Value>(proc, Branch, Origin(), condition);

    thenCase->clearSuccessors();
    thenCase->appendNew<Value>(proc, Return, Origin(), thenCase->appendNew<Const32Value>(proc, Origin(), 1));

    elseCase->clearSuccessors();
    elseCase->appendNew<Value>(proc, Return, Origin(), elseCase->appendNew<Const32Value>(proc, Origin(), 0));

    int32_t expected = (a == b || c == d || e == f) ? 1 : 0;
    CHECK_EQ(compileAndRun<int32_t>(proc, a, b, c, d, e, f), expected);
}

void testCCmpAndOr32(int32_t a, int32_t b, int32_t c, int32_t d, int32_t e, int32_t f)
{
    // Test: ((a == b) && (c == d)) || (e == f)
    // Mixed pattern: AND then OR
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    BasicBlock* thenCase = proc.addBlock();
    BasicBlock* elseCase = proc.addBlock();
    auto arguments = cCallArgumentValues<int32_t, int32_t, int32_t, int32_t, int32_t, int32_t>(proc, root);

    Value* cmp1 = root->appendNew<Value>(proc, Equal, Origin(), arguments[0], arguments[1]);
    Value* cmp2 = root->appendNew<Value>(proc, Equal, Origin(), arguments[2], arguments[3]);
    Value* and1 = root->appendNew<Value>(proc, BitAnd, Origin(), cmp1, cmp2);
    Value* cmp3 = root->appendNew<Value>(proc, Equal, Origin(), arguments[4], arguments[5]);
    Value* condition = root->appendNew<Value>(proc, BitOr, Origin(), and1, cmp3);

    root->setSuccessors(FrequentedBlock(thenCase), FrequentedBlock(elseCase));
    root->appendNew<Value>(proc, Branch, Origin(), condition);

    thenCase->clearSuccessors();
    thenCase->appendNew<Value>(proc, Return, Origin(), thenCase->appendNew<Const32Value>(proc, Origin(), 1));

    elseCase->clearSuccessors();
    elseCase->appendNew<Value>(proc, Return, Origin(), elseCase->appendNew<Const32Value>(proc, Origin(), 0));

    int32_t expected = ((a == b && c == d) || e == f) ? 1 : 0;
    CHECK_EQ(compileAndRun<int32_t>(proc, a, b, c, d, e, f), expected);
}

// Tests for ccmn (conditional compare with negative immediates)
void testCCmnAnd32WithNegativeImm(int32_t a, int32_t b)
{
    // Test: (a > 10) && (b == -5)
    // The second comparison should use ccmn with immediate 5
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    BasicBlock* thenCase = proc.addBlock();
    BasicBlock* elseCase = proc.addBlock();
    auto arguments = cCallArgumentValues<int32_t, int32_t>(proc, root);

    Value* cmp1 = root->appendNew<Value>(proc, GreaterThan, Origin(), arguments[0], root->appendNew<Const32Value>(proc, Origin(), 10));
    Value* cmp2 = root->appendNew<Value>(proc, Equal, Origin(), arguments[1], root->appendNew<Const32Value>(proc, Origin(), -5));
    Value* condition = root->appendNew<Value>(proc, BitAnd, Origin(), cmp1, cmp2);

    root->setSuccessors(FrequentedBlock(thenCase), FrequentedBlock(elseCase));
    root->appendNew<Value>(proc, Branch, Origin(), condition);

    thenCase->clearSuccessors();
    thenCase->appendNew<Value>(proc, Return, Origin(), thenCase->appendNew<Const32Value>(proc, Origin(), 1));

    elseCase->clearSuccessors();
    elseCase->appendNew<Value>(proc, Return, Origin(), elseCase->appendNew<Const32Value>(proc, Origin(), 0));

    int32_t expected = (a > 10 && b == -5) ? 1 : 0;
    CHECK_EQ(compileAndRun<int32_t>(proc, a, b), expected);
}

void testCCmnAnd64WithNegativeImm(int64_t a, int64_t b)
{
    // Test: (a > 10) && (b == -31)
    // The second comparison should use ccmn with immediate 31
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    BasicBlock* thenCase = proc.addBlock();
    BasicBlock* elseCase = proc.addBlock();
    auto arguments = cCallArgumentValues<int64_t, int64_t>(proc, root);

    Value* cmp1 = root->appendNew<Value>(proc, GreaterThan, Origin(), arguments[0], root->appendNew<Const64Value>(proc, Origin(), 10));
    Value* cmp2 = root->appendNew<Value>(proc, Equal, Origin(), arguments[1], root->appendNew<Const64Value>(proc, Origin(), -31));
    Value* condition = root->appendNew<Value>(proc, BitAnd, Origin(), cmp1, cmp2);

    root->setSuccessors(FrequentedBlock(thenCase), FrequentedBlock(elseCase));
    root->appendNew<Value>(proc, Branch, Origin(), condition);

    thenCase->clearSuccessors();
    thenCase->appendNew<Value>(proc, Return, Origin(), thenCase->appendNew<Const32Value>(proc, Origin(), 1));

    elseCase->clearSuccessors();
    elseCase->appendNew<Value>(proc, Return, Origin(), elseCase->appendNew<Const32Value>(proc, Origin(), 0));

    int32_t expected = (a > 10 && b == -31) ? 1 : 0;
    CHECK_EQ(compileAndRun<int32_t>(proc, a, b), expected);
}

void testCCmpWithLargePositiveImm(int32_t a, int32_t b)
{
    // Test: (a > 10) && (b == 100)
    // The second comparison should use a register (100 > 31)
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    BasicBlock* thenCase = proc.addBlock();
    BasicBlock* elseCase = proc.addBlock();
    auto arguments = cCallArgumentValues<int32_t, int32_t>(proc, root);

    Value* cmp1 = root->appendNew<Value>(proc, GreaterThan, Origin(), arguments[0], root->appendNew<Const32Value>(proc, Origin(), 10));
    Value* cmp2 = root->appendNew<Value>(proc, Equal, Origin(), arguments[1], root->appendNew<Const32Value>(proc, Origin(), 100));
    Value* condition = root->appendNew<Value>(proc, BitAnd, Origin(), cmp1, cmp2);

    root->setSuccessors(FrequentedBlock(thenCase), FrequentedBlock(elseCase));
    root->appendNew<Value>(proc, Branch, Origin(), condition);

    thenCase->clearSuccessors();
    thenCase->appendNew<Value>(proc, Return, Origin(), thenCase->appendNew<Const32Value>(proc, Origin(), 1));

    elseCase->clearSuccessors();
    elseCase->appendNew<Value>(proc, Return, Origin(), elseCase->appendNew<Const32Value>(proc, Origin(), 0));

    int32_t expected = (a > 10 && b == 100) ? 1 : 0;
    CHECK_EQ(compileAndRun<int32_t>(proc, a, b), expected);
}

void testCCmpWithLargeNegativeImm(int32_t a, int32_t b)
{
    // Test: (a > 10) && (b == -100)
    // The second comparison should use a register (-100 < -31)
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    BasicBlock* thenCase = proc.addBlock();
    BasicBlock* elseCase = proc.addBlock();
    auto arguments = cCallArgumentValues<int32_t, int32_t>(proc, root);

    Value* cmp1 = root->appendNew<Value>(proc, GreaterThan, Origin(), arguments[0], root->appendNew<Const32Value>(proc, Origin(), 10));
    Value* cmp2 = root->appendNew<Value>(proc, Equal, Origin(), arguments[1], root->appendNew<Const32Value>(proc, Origin(), -100));
    Value* condition = root->appendNew<Value>(proc, BitAnd, Origin(), cmp1, cmp2);

    root->setSuccessors(FrequentedBlock(thenCase), FrequentedBlock(elseCase));
    root->appendNew<Value>(proc, Branch, Origin(), condition);

    thenCase->clearSuccessors();
    thenCase->appendNew<Value>(proc, Return, Origin(), thenCase->appendNew<Const32Value>(proc, Origin(), 1));

    elseCase->clearSuccessors();
    elseCase->appendNew<Value>(proc, Return, Origin(), elseCase->appendNew<Const32Value>(proc, Origin(), 0));

    int32_t expected = (a > 10 && b == -100) ? 1 : 0;
    CHECK_EQ(compileAndRun<int32_t>(proc, a, b), expected);
}

// Tests for ccmp optimization: smart operand ordering
// This test ensures that when the first comparison has a small immediate (5)
// and the second has a large immediate (1000), we swap them so that the
// large immediate goes into cmp (which has wider immediate range) and the
// small immediate goes into ccmp.
void testCCmpSmartOperandOrdering32(int32_t a, int32_t b)
{
    // Test: (a == 5) && (b == 1000)
    // Should be optimized to: cmp b, 1000; ccmp a, 5, ...
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    BasicBlock* thenCase = proc.addBlock();
    BasicBlock* elseCase = proc.addBlock();
    auto arguments = cCallArgumentValues<int32_t, int32_t>(proc, root);

    Value* cmp1 = root->appendNew<Value>(proc, Equal, Origin(), arguments[0], root->appendNew<Const32Value>(proc, Origin(), 5));
    Value* cmp2 = root->appendNew<Value>(proc, Equal, Origin(), arguments[1], root->appendNew<Const32Value>(proc, Origin(), 1000));
    Value* condition = root->appendNew<Value>(proc, BitAnd, Origin(), cmp1, cmp2);

    root->setSuccessors(FrequentedBlock(thenCase), FrequentedBlock(elseCase));
    root->appendNew<Value>(proc, Branch, Origin(), condition);

    thenCase->clearSuccessors();
    thenCase->appendNew<Value>(proc, Return, Origin(), thenCase->appendNew<Const32Value>(proc, Origin(), 1));

    elseCase->clearSuccessors();
    elseCase->appendNew<Value>(proc, Return, Origin(), elseCase->appendNew<Const32Value>(proc, Origin(), 0));

    int32_t expected = (a == 5 && b == 1000) ? 1 : 0;
    CHECK_EQ(compileAndRun<int32_t>(proc, a, b), expected);
}

void testCCmpSmartOperandOrdering64(int64_t a, int64_t b)
{
    // Test: (a == 10) && (b == 5000)
    // Should be optimized to: cmp b, 5000; ccmp a, 10, ...
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    BasicBlock* thenCase = proc.addBlock();
    BasicBlock* elseCase = proc.addBlock();
    auto arguments = cCallArgumentValues<int64_t, int64_t>(proc, root);

    Value* cmp1 = root->appendNew<Value>(proc, Equal, Origin(), arguments[0], root->appendNew<Const64Value>(proc, Origin(), 10));
    Value* cmp2 = root->appendNew<Value>(proc, Equal, Origin(), arguments[1], root->appendNew<Const64Value>(proc, Origin(), 5000));
    Value* condition = root->appendNew<Value>(proc, BitAnd, Origin(), cmp1, cmp2);

    root->setSuccessors(FrequentedBlock(thenCase), FrequentedBlock(elseCase));
    root->appendNew<Value>(proc, Branch, Origin(), condition);

    thenCase->clearSuccessors();
    thenCase->appendNew<Value>(proc, Return, Origin(), thenCase->appendNew<Const32Value>(proc, Origin(), 1));

    elseCase->clearSuccessors();
    elseCase->appendNew<Value>(proc, Return, Origin(), elseCase->appendNew<Const32Value>(proc, Origin(), 0));

    int32_t expected = (a == 10 && b == 5000) ? 1 : 0;
    CHECK_EQ(compileAndRun<int32_t>(proc, a, b), expected);
}

// Tests for ccmp optimization: operand commutation within ccmp
// This test ensures that if the left operand of a comparison is a small immediate,
// we swap the operands to put the immediate on the right where it can be encoded.
void testCCmpOperandCommutation32(int32_t a, int32_t b)
{
    // Test: (15 == a) && (b > 100)
    // The first comparison should commute to (a == 15)
    // and optimize to: cmp a, 15; ccmp b, 100, ...
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    BasicBlock* thenCase = proc.addBlock();
    BasicBlock* elseCase = proc.addBlock();
    auto arguments = cCallArgumentValues<int32_t, int32_t>(proc, root);

    Value* cmp1 = root->appendNew<Value>(proc, Equal, Origin(), root->appendNew<Const32Value>(proc, Origin(), 15), arguments[0]);
    Value* cmp2 = root->appendNew<Value>(proc, GreaterThan, Origin(), arguments[1], root->appendNew<Const32Value>(proc, Origin(), 100));
    Value* condition = root->appendNew<Value>(proc, BitAnd, Origin(), cmp1, cmp2);

    root->setSuccessors(FrequentedBlock(thenCase), FrequentedBlock(elseCase));
    root->appendNew<Value>(proc, Branch, Origin(), condition);

    thenCase->clearSuccessors();
    thenCase->appendNew<Value>(proc, Return, Origin(), thenCase->appendNew<Const32Value>(proc, Origin(), 1));

    elseCase->clearSuccessors();
    elseCase->appendNew<Value>(proc, Return, Origin(), elseCase->appendNew<Const32Value>(proc, Origin(), 0));

    int32_t expected = (15 == a && b > 100) ? 1 : 0;
    CHECK_EQ(compileAndRun<int32_t>(proc, a, b), expected);
}

void testCCmpOperandCommutation64(int64_t a, int64_t b)
{
    // Test: (a < 50) && (20 == b)
    // The second comparison should commute in the ccmp to (b == 20)
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    BasicBlock* thenCase = proc.addBlock();
    BasicBlock* elseCase = proc.addBlock();
    auto arguments = cCallArgumentValues<int64_t, int64_t>(proc, root);

    Value* cmp1 = root->appendNew<Value>(proc, LessThan, Origin(), arguments[0], root->appendNew<Const64Value>(proc, Origin(), 50));
    Value* cmp2 = root->appendNew<Value>(proc, Equal, Origin(), root->appendNew<Const64Value>(proc, Origin(), 20), arguments[1]);
    Value* condition = root->appendNew<Value>(proc, BitAnd, Origin(), cmp1, cmp2);

    root->setSuccessors(FrequentedBlock(thenCase), FrequentedBlock(elseCase));
    root->appendNew<Value>(proc, Branch, Origin(), condition);

    thenCase->clearSuccessors();
    thenCase->appendNew<Value>(proc, Return, Origin(), thenCase->appendNew<Const32Value>(proc, Origin(), 1));

    elseCase->clearSuccessors();
    elseCase->appendNew<Value>(proc, Return, Origin(), elseCase->appendNew<Const32Value>(proc, Origin(), 0));

    int32_t expected = (a < 50 && 20 == b) ? 1 : 0;
    CHECK_EQ(compileAndRun<int32_t>(proc, a, b), expected);
}

// Combined test: both smart ordering and operand commutation
void testCCmpCombinedOptimizations(int32_t a, int32_t b)
{
    // Test: (10 == a) && (b == 2000)
    // First comparison has commutable immediate on left
    // Second comparison has large immediate
    // Should optimize to: cmp b, 2000; ccmp a, 10, ...
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    BasicBlock* thenCase = proc.addBlock();
    BasicBlock* elseCase = proc.addBlock();
    auto arguments = cCallArgumentValues<int32_t, int32_t>(proc, root);

    Value* cmp1 = root->appendNew<Value>(proc, Equal, Origin(), root->appendNew<Const32Value>(proc, Origin(), 10), arguments[0]);
    Value* cmp2 = root->appendNew<Value>(proc, Equal, Origin(), arguments[1], root->appendNew<Const32Value>(proc, Origin(), 2000));
    Value* condition = root->appendNew<Value>(proc, BitAnd, Origin(), cmp1, cmp2);

    root->setSuccessors(FrequentedBlock(thenCase), FrequentedBlock(elseCase));
    root->appendNew<Value>(proc, Branch, Origin(), condition);

    thenCase->clearSuccessors();
    thenCase->appendNew<Value>(proc, Return, Origin(), thenCase->appendNew<Const32Value>(proc, Origin(), 1));

    elseCase->clearSuccessors();
    elseCase->appendNew<Value>(proc, Return, Origin(), elseCase->appendNew<Const32Value>(proc, Origin(), 0));

    int32_t expected = (10 == a && b == 2000) ? 1 : 0;
    CHECK_EQ(compileAndRun<int32_t>(proc, a, b), expected);
}

// Test for zero register optimization
void testCCmpZeroRegisterOptimization32(int32_t a, int32_t b)
{
    // Test: (a == 0) && (b > 5)
    // The first comparison should use the zero register for 0
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    BasicBlock* thenCase = proc.addBlock();
    BasicBlock* elseCase = proc.addBlock();
    auto arguments = cCallArgumentValues<int32_t, int32_t>(proc, root);

    Value* cmp1 = root->appendNew<Value>(proc, Equal, Origin(), arguments[0], root->appendNew<Const32Value>(proc, Origin(), 0));
    Value* cmp2 = root->appendNew<Value>(proc, GreaterThan, Origin(), arguments[1], root->appendNew<Const32Value>(proc, Origin(), 5));
    Value* condition = root->appendNew<Value>(proc, BitAnd, Origin(), cmp1, cmp2);

    root->setSuccessors(FrequentedBlock(thenCase), FrequentedBlock(elseCase));
    root->appendNew<Value>(proc, Branch, Origin(), condition);

    thenCase->clearSuccessors();
    thenCase->appendNew<Value>(proc, Return, Origin(), thenCase->appendNew<Const32Value>(proc, Origin(), 1));

    elseCase->clearSuccessors();
    elseCase->appendNew<Value>(proc, Return, Origin(), elseCase->appendNew<Const32Value>(proc, Origin(), 0));

    int32_t expected = (a == 0 && b > 5) ? 1 : 0;
    CHECK_EQ(compileAndRun<int32_t>(proc, a, b), expected);
}

void testCCmpZeroRegisterOptimization64(int64_t a, int64_t b)
{
    // Test: (0 == a) && (b < 100)
    // The first comparison should use the zero register, and also test commutation
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    BasicBlock* thenCase = proc.addBlock();
    BasicBlock* elseCase = proc.addBlock();
    auto arguments = cCallArgumentValues<int64_t, int64_t>(proc, root);

    Value* cmp1 = root->appendNew<Value>(proc, Equal, Origin(), root->appendNew<Const64Value>(proc, Origin(), 0), arguments[0]);
    Value* cmp2 = root->appendNew<Value>(proc, LessThan, Origin(), arguments[1], root->appendNew<Const64Value>(proc, Origin(), 100));
    Value* condition = root->appendNew<Value>(proc, BitAnd, Origin(), cmp1, cmp2);

    root->setSuccessors(FrequentedBlock(thenCase), FrequentedBlock(elseCase));
    root->appendNew<Value>(proc, Branch, Origin(), condition);

    thenCase->clearSuccessors();
    thenCase->appendNew<Value>(proc, Return, Origin(), thenCase->appendNew<Const32Value>(proc, Origin(), 1));

    elseCase->clearSuccessors();
    elseCase->appendNew<Value>(proc, Return, Origin(), elseCase->appendNew<Const32Value>(proc, Origin(), 0));

    int32_t expected = (0 == a && b < 100) ? 1 : 0;
    CHECK_EQ(compileAndRun<int32_t>(proc, a, b), expected);
}

// Mixed AND/OR tests - these now work with tree-based processing
void testCCmpMixedAndOr32(int32_t a, int32_t b, int32_t c)
{
    // Test: (a == b && b == c) || (a > 100)
    // Left child is AND (logic op), right child is comparison
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    BasicBlock* thenCase = proc.addBlock();
    BasicBlock* elseCase = proc.addBlock();
    auto arguments = cCallArgumentValues<int32_t, int32_t, int32_t>(proc, root);

    Value* cmp1 = root->appendNew<Value>(proc, Equal, Origin(), arguments[0], arguments[1]);
    Value* cmp2 = root->appendNew<Value>(proc, Equal, Origin(), arguments[1], arguments[2]);
    Value* andVal = root->appendNew<Value>(proc, BitAnd, Origin(), cmp1, cmp2);
    Value* cmp3 = root->appendNew<Value>(proc, GreaterThan, Origin(), arguments[0], root->appendNew<Const32Value>(proc, Origin(), 100));
    Value* condition = root->appendNew<Value>(proc, BitOr, Origin(), andVal, cmp3);

    root->setSuccessors(FrequentedBlock(thenCase), FrequentedBlock(elseCase));
    root->appendNew<Value>(proc, Branch, Origin(), condition);

    thenCase->clearSuccessors();
    thenCase->appendNew<Value>(proc, Return, Origin(), thenCase->appendNew<Const32Value>(proc, Origin(), 1));

    elseCase->clearSuccessors();
    elseCase->appendNew<Value>(proc, Return, Origin(), elseCase->appendNew<Const32Value>(proc, Origin(), 0));

    int32_t expected = ((a == b && b == c) || a > 100) ? 1 : 0;
    CHECK_EQ(compileAndRun<int32_t>(proc, a, b, c), expected);
}

void testCCmpMixedOrAnd32(int32_t a, int32_t b, int32_t c)
{
    // Test: (a < 0) || (b == c && c > 50)
    // Left child is comparison, right child is AND (logic op)
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    BasicBlock* thenCase = proc.addBlock();
    BasicBlock* elseCase = proc.addBlock();
    auto arguments = cCallArgumentValues<int32_t, int32_t, int32_t>(proc, root);

    Value* cmp1 = root->appendNew<Value>(proc, LessThan, Origin(), arguments[0], root->appendNew<Const32Value>(proc, Origin(), 0));
    Value* cmp2 = root->appendNew<Value>(proc, Equal, Origin(), arguments[1], arguments[2]);
    Value* cmp3 = root->appendNew<Value>(proc, GreaterThan, Origin(), arguments[2], root->appendNew<Const32Value>(proc, Origin(), 50));
    Value* andVal = root->appendNew<Value>(proc, BitAnd, Origin(), cmp2, cmp3);
    Value* condition = root->appendNew<Value>(proc, BitOr, Origin(), cmp1, andVal);

    root->setSuccessors(FrequentedBlock(thenCase), FrequentedBlock(elseCase));
    root->appendNew<Value>(proc, Branch, Origin(), condition);

    thenCase->clearSuccessors();
    thenCase->appendNew<Value>(proc, Return, Origin(), thenCase->appendNew<Const32Value>(proc, Origin(), 1));

    elseCase->clearSuccessors();
    elseCase->appendNew<Value>(proc, Return, Origin(), elseCase->appendNew<Const32Value>(proc, Origin(), 0));

    int32_t expected = (a < 0 || (b == c && c > 50)) ? 1 : 0;
    CHECK_EQ(compileAndRun<int32_t>(proc, a, b, c), expected);
}

void testCCmpNegatedAnd32(int32_t a, int32_t b)
{
    // Test: !(a > 10 && b == 20)
    // This becomes: (a > 10 && b == 20) == 0
    // Should be optimized with ccmp and final condition negation
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    BasicBlock* thenCase = proc.addBlock();
    BasicBlock* elseCase = proc.addBlock();

    Value* arg1 = root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0);
    Value* arg2 = root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR1);

    Value* greaterThan10 = root->appendNew<Value>(
        proc, GreaterThan, Origin(),
        root->appendNew<Value>(proc, Trunc, Origin(), arg1),
        root->appendNew<Const32Value>(proc, Origin(), 10));

    Value* equal20 = root->appendNew<Value>(
        proc, Equal, Origin(),
        root->appendNew<Value>(proc, Trunc, Origin(), arg2),
        root->appendNew<Const32Value>(proc, Origin(), 20));

    Value* andResult = root->appendNew<Value>(
        proc, BitAnd, Origin(),
        greaterThan10,
        equal20);

    // Negation: andResult == 0
    Value* negated = root->appendNew<Value>(
        proc, Equal, Origin(),
        andResult,
        root->appendNew<Const32Value>(proc, Origin(), 0));

    root->setSuccessors(FrequentedBlock(thenCase), FrequentedBlock(elseCase));
    root->appendNew<Value>(proc, Branch, Origin(), negated);

    thenCase->clearSuccessors();
    thenCase->appendNew<Value>(proc, Return, Origin(), thenCase->appendNew<Const32Value>(proc, Origin(), 1));

    elseCase->clearSuccessors();
    elseCase->appendNew<Value>(proc, Return, Origin(), elseCase->appendNew<Const32Value>(proc, Origin(), 0));

    int32_t expected = !(a > 10 && b == 20) ? 1 : 0;
    CHECK_EQ(compileAndRun<int32_t>(proc, a, b), expected);
}

void testCCmpNegatedOr32(int32_t a, int32_t b)
{
    // Test: !(a < 5 || b >= 100)
    // This becomes: (a < 5 || b >= 100) == 0
    // Should be optimized with ccmp and final condition negation
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    BasicBlock* thenCase = proc.addBlock();
    BasicBlock* elseCase = proc.addBlock();

    Value* arg1 = root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0);
    Value* arg2 = root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR1);

    Value* lessThan5 = root->appendNew<Value>(
        proc, LessThan, Origin(),
        root->appendNew<Value>(proc, Trunc, Origin(), arg1),
        root->appendNew<Const32Value>(proc, Origin(), 5));

    Value* greaterOrEqual100 = root->appendNew<Value>(
        proc, GreaterEqual, Origin(),
        root->appendNew<Value>(proc, Trunc, Origin(), arg2),
        root->appendNew<Const32Value>(proc, Origin(), 100));

    Value* orResult = root->appendNew<Value>(
        proc, BitOr, Origin(),
        lessThan5,
        greaterOrEqual100);

    // Negation: orResult == 0
    Value* negated = root->appendNew<Value>(
        proc, Equal, Origin(),
        orResult,
        root->appendNew<Const32Value>(proc, Origin(), 0));

    root->setSuccessors(FrequentedBlock(thenCase), FrequentedBlock(elseCase));
    root->appendNew<Value>(proc, Branch, Origin(), negated);

    thenCase->clearSuccessors();
    thenCase->appendNew<Value>(proc, Return, Origin(), thenCase->appendNew<Const32Value>(proc, Origin(), 1));

    elseCase->clearSuccessors();
    elseCase->appendNew<Value>(proc, Return, Origin(), elseCase->appendNew<Const32Value>(proc, Origin(), 0));

    int32_t expected = !(a < 5 || b >= 100) ? 1 : 0;
    CHECK_EQ(compileAndRun<int32_t>(proc, a, b), expected);
}

// Test for mixed-width compare chains (32-bit and 64-bit comparisons in same chain)
// This tests the per-ccmp width handling fix
void testCCmpMixedWidth32And64(int32_t a, int64_t b, int32_t c)
{
    // Test: (a == 5) && (b == 1000) && (c == 10)
    // First is 32-bit, second is 64-bit, third is 32-bit
    // Each ccmp must use its own width for the opcode
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    BasicBlock* thenCase = proc.addBlock();
    BasicBlock* elseCase = proc.addBlock();
    auto arguments = cCallArgumentValues<int32_t, int64_t, int32_t>(proc, root);

    // arguments[0] is Int32, arguments[1] is Int64, arguments[2] is Int32
    Value* cmp1 = root->appendNew<Value>(proc, Equal, Origin(),
        arguments[0],
        root->appendNew<Const32Value>(proc, Origin(), 5));

    Value* cmp2 = root->appendNew<Value>(proc, Equal, Origin(),
        arguments[1],
        root->appendNew<Const64Value>(proc, Origin(), 1000));

    Value* and1 = root->appendNew<Value>(proc, BitAnd, Origin(), cmp1, cmp2);

    Value* cmp3 = root->appendNew<Value>(proc, Equal, Origin(),
        arguments[2],
        root->appendNew<Const32Value>(proc, Origin(), 10));

    Value* condition = root->appendNew<Value>(proc, BitAnd, Origin(), and1, cmp3);

    root->setSuccessors(FrequentedBlock(thenCase), FrequentedBlock(elseCase));
    root->appendNew<Value>(proc, Branch, Origin(), condition);

    thenCase->clearSuccessors();
    thenCase->appendNew<Value>(proc, Return, Origin(), thenCase->appendNew<Const32Value>(proc, Origin(), 1));

    elseCase->clearSuccessors();
    elseCase->appendNew<Value>(proc, Return, Origin(), elseCase->appendNew<Const32Value>(proc, Origin(), 0));

    int32_t expected = (a == 5 && b == 1000 && c == 10) ? 1 : 0;
    CHECK_EQ(compileAndRun<int32_t>(proc, a, b, c), expected);
}

void testCCmpMixedWidth64And32(int64_t a, int32_t b)
{
    // Test: (a == 5000) && (b == 10)
    // First is 64-bit, second is 32-bit
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    BasicBlock* thenCase = proc.addBlock();
    BasicBlock* elseCase = proc.addBlock();
    auto arguments = cCallArgumentValues<int64_t, int32_t>(proc, root);

    Value* cmp1 = root->appendNew<Value>(proc, Equal, Origin(),
        arguments[0],
        root->appendNew<Const64Value>(proc, Origin(), 5000));

    Value* cmp2 = root->appendNew<Value>(proc, Equal, Origin(),
        arguments[1],
        root->appendNew<Const32Value>(proc, Origin(), 10));

    Value* condition = root->appendNew<Value>(proc, BitAnd, Origin(), cmp1, cmp2);

    root->setSuccessors(FrequentedBlock(thenCase), FrequentedBlock(elseCase));
    root->appendNew<Value>(proc, Branch, Origin(), condition);

    thenCase->clearSuccessors();
    thenCase->appendNew<Value>(proc, Return, Origin(), thenCase->appendNew<Const32Value>(proc, Origin(), 1));

    elseCase->clearSuccessors();
    elseCase->appendNew<Value>(proc, Return, Origin(), elseCase->appendNew<Const32Value>(proc, Origin(), 0));

    int32_t expected = (a == 5000 && b == 10) ? 1 : 0;
    CHECK_EQ(compileAndRun<int32_t>(proc, a, b), expected);
}

// Regression for findCompareChain rollback: pre-fix, BitOr's BitXor-child
// failure leaked an inner logic node, dropping LessThan from the chain.
void testCCmpChainRollback(int32_t i, int32_t len, int32_t a, int32_t b, int32_t c, int32_t d)
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    BasicBlock* thenCase = proc.addBlock();
    BasicBlock* elseCase = proc.addBlock();
    auto arguments = cCallArgumentValues<int32_t, int32_t, int32_t, int32_t, int32_t, int32_t>(proc, root);

    Value* iLtLen = root->appendNew<Value>(proc, LessThan, Origin(), arguments[0], arguments[1]);
    Value* eqAB = root->appendNew<Value>(proc, Equal, Origin(), arguments[2], arguments[3]);
    Value* eqCD = root->appendNew<Value>(proc, Equal, Origin(), arguments[4], arguments[5]);
    Value* innerAnd = root->appendNew<Value>(proc, BitAnd, Origin(), eqAB, eqCD);
    Value* xorAC = root->appendNew<Value>(proc, BitXor, Origin(), arguments[2], arguments[4]);
    Value* outerOr = root->appendNew<Value>(proc, BitOr, Origin(), innerAnd, xorAC);
    Value* eqZero = root->appendNew<Value>(proc, Equal, Origin(),
        outerOr, root->appendNew<Const32Value>(proc, Origin(), 0));
    Value* condition = root->appendNew<Value>(proc, BitAnd, Origin(), iLtLen, eqZero);

    root->setSuccessors(FrequentedBlock(thenCase), FrequentedBlock(elseCase));
    root->appendNew<Value>(proc, Branch, Origin(), condition);

    thenCase->clearSuccessors();
    thenCase->appendNew<Value>(proc, Return, Origin(), thenCase->appendNew<Const32Value>(proc, Origin(), 1));

    elseCase->clearSuccessors();
    elseCase->appendNew<Value>(proc, Return, Origin(), elseCase->appendNew<Const32Value>(proc, Origin(), 0));

    int zero = 0; // style checker complains about == 0
    int32_t expected = (i < len && ((((a == b) & (c == d)) | (a ^ c)) == zero)) ? 1 : 0;
    CHECK_EQ(compileAndRun<int32_t>(proc, i, len, a, b, c, d), expected);
}

void testConstDoubleZero()
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    root->clearSuccessors();
    root->appendNew<Value>(proc, Return, Origin(), root->appendNew<ConstDoubleValue>(proc, Origin(), 0.0));
    CHECK_EQ(compileAndRun<double>(proc), 0.0);
}

void testConstDoubleNegativeZero()
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    root->clearSuccessors();
    root->appendNew<Value>(proc, Return, Origin(), root->appendNew<ConstDoubleValue>(proc, Origin(), -0.0));
    double result = compileAndRun<double>(proc);
    CHECK_EQ(std::bit_cast<uint64_t>(result), 0x8000000000000000ULL);
}

void testConstFloatZero()
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    root->clearSuccessors();
    root->appendNew<Value>(proc, Return, Origin(), root->appendNew<ConstFloatValue>(proc, Origin(), 0.0f));
    CHECK_EQ(compileAndRun<float>(proc), 0.0f);
}

void testConstFloatNegativeZero()
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    root->clearSuccessors();
    root->appendNew<Value>(proc, Return, Origin(), root->appendNew<ConstFloatValue>(proc, Origin(), -0.0f));
    float result = compileAndRun<float>(proc);
    CHECK_EQ(std::bit_cast<uint32_t>(result), 0x80000000U);
}

void testConstDoubleAddZero()
{
    auto test = [&] (double input, double expected) {
        Procedure proc;
        BasicBlock* root = proc.addBlock();
        auto arguments = cCallArgumentValues<double>(proc, root);
        Value* zero = root->appendNew<ConstDoubleValue>(proc, Origin(), 0.0);
        Value* result = root->appendNew<Value>(proc, Add, Origin(), arguments[0], zero);
        root->clearSuccessors();
        root->appendNew<Value>(proc, Return, Origin(), result);
        CHECK_EQ(compileAndRun<double>(proc, input), expected);
    };

    test(2.5, 2.5);
    test(-3.14, -3.14);
    test(0.0, 0.0);
}

void testConstFloatAddZero()
{
    auto test = [&] (float input, float expected) {
        Procedure proc;
        BasicBlock* root = proc.addBlock();
        auto arguments = cCallArgumentValues<float>(proc, root);
        Value* zero = root->appendNew<ConstFloatValue>(proc, Origin(), 0.0f);
        Value* result = root->appendNew<Value>(proc, Add, Origin(), arguments[0], zero);
        root->clearSuccessors();
        root->appendNew<Value>(proc, Return, Origin(), result);
        CHECK_EQ(compileAndRun<float>(proc, input), expected);
    };

    test(2.5f, 2.5f);
    test(-3.14f, -3.14f);
    test(0.0f, 0.0f);
}

void testConstDoubleCompareZero()
{
    auto test = [&] (double input, int32_t expected) {
        Procedure proc;
        BasicBlock* root = proc.addBlock();
        auto arguments = cCallArgumentValues<double>(proc, root);
        Value* zero = root->appendNew<ConstDoubleValue>(proc, Origin(), 0.0);
        Value* result = root->appendNew<Value>(proc, Equal, Origin(), arguments[0], zero);
        root->clearSuccessors();
        root->appendNew<Value>(proc, Return, Origin(), result);
        CHECK_EQ(compileAndRun<int32_t>(proc, input), expected);
    };

    test(0.0, 1);
    test(-0.0, 1); // -0.0 == 0.0
    test(1.0, 0);
    test(-1.0, 0);
}

void testConstFloatCompareZero()
{
    auto test = [&] (float input, int32_t expected) {
        Procedure proc;
        BasicBlock* root = proc.addBlock();
        auto arguments = cCallArgumentValues<float>(proc, root);
        Value* zero = root->appendNew<ConstFloatValue>(proc, Origin(), 0.0f);
        Value* result = root->appendNew<Value>(proc, Equal, Origin(), arguments[0], zero);
        root->clearSuccessors();
        root->appendNew<Value>(proc, Return, Origin(), result);
        CHECK_EQ(compileAndRun<int32_t>(proc, input), expected);
    };

    test(0.0f, 1);
    test(-0.0f, 1); // -0.0f == 0.0f
    test(1.0f, 0);
    test(-1.0f, 0);
}

void testConstDoubleSelectZero()
{
    auto test = [&] (int32_t selector, double input, double expected) {
        Procedure proc;
        BasicBlock* root = proc.addBlock();
        auto arguments = cCallArgumentValues<int32_t, double>(proc, root);
        Value* zero = root->appendNew<ConstDoubleValue>(proc, Origin(), 0.0);
        Value* result = root->appendNew<Value>(proc, Select, Origin(), arguments[0], arguments[1], zero);
        root->clearSuccessors();
        root->appendNew<Value>(proc, Return, Origin(), result);
        CHECK_EQ(compileAndRun<double>(proc, selector, input), expected);
    };

    test(1, 2.5, 2.5);
    test(0, 2.5, 0.0);
}

void testConstFloatSelectZero()
{
    auto test = [&] (int32_t selector, float input, float expected) {
        Procedure proc;
        BasicBlock* root = proc.addBlock();
        auto arguments = cCallArgumentValues<int32_t, float>(proc, root);
        Value* zero = root->appendNew<ConstFloatValue>(proc, Origin(), 0.0f);
        Value* result = root->appendNew<Value>(proc, Select, Origin(), arguments[0], arguments[1], zero);
        root->clearSuccessors();
        root->appendNew<Value>(proc, Return, Origin(), result);
        CHECK_EQ(compileAndRun<float>(proc, selector, input), expected);
    };

    test(1, 2.5f, 2.5f);
    test(0, 2.5f, 0.0f);
}

void testConstDoubleMultipleZeroUses()
{
    // Test that multiple uses of zero constant work correctly
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    auto arguments = cCallArgumentValues<double, double>(proc, root);
    Value* zero = root->appendNew<ConstDoubleValue>(proc, Origin(), 0.0);

    // Use zero multiple times: (a + 0) + (b + 0)
    Value* aPlusZero = root->appendNew<Value>(proc, Add, Origin(), arguments[0], zero);
    Value* bPlusZero = root->appendNew<Value>(proc, Add, Origin(), arguments[1], zero);
    Value* result = root->appendNew<Value>(proc, Add, Origin(), aPlusZero, bPlusZero);

    root->clearSuccessors();
    root->appendNew<Value>(proc, Return, Origin(), result);

    CHECK_EQ(compileAndRun<double>(proc, 2.5, 3.5), 6.0);
}

void testConstFloatMultipleZeroUses()
{
    // Test that multiple uses of zero constant work correctly
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    auto arguments = cCallArgumentValues<float, float>(proc, root);
    Value* zero = root->appendNew<ConstFloatValue>(proc, Origin(), 0.0f);

    // Use zero multiple times: (a + 0) + (b + 0)
    Value* aPlusZero = root->appendNew<Value>(proc, Add, Origin(), arguments[0], zero);
    Value* bPlusZero = root->appendNew<Value>(proc, Add, Origin(), arguments[1], zero);
    Value* result = root->appendNew<Value>(proc, Add, Origin(), aPlusZero, bPlusZero);

    root->clearSuccessors();
    root->appendNew<Value>(proc, Return, Origin(), result);

    CHECK_EQ(compileAndRun<float>(proc, 2.5f, 3.5f), 6.0f);
}

void testFCCmpAndDouble(double a, double b, double c, double d)
{
    // Test: (a == b) && (c == d)
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    BasicBlock* thenCase = proc.addBlock();
    BasicBlock* elseCase = proc.addBlock();
    auto arguments = cCallArgumentValues<double, double, double, double>(proc, root);

    Value* cmp1 = root->appendNew<Value>(proc, Equal, Origin(), arguments[0], arguments[1]);
    Value* cmp2 = root->appendNew<Value>(proc, Equal, Origin(), arguments[2], arguments[3]);
    Value* condition = root->appendNew<Value>(proc, BitAnd, Origin(), cmp1, cmp2);

    root->setSuccessors(FrequentedBlock(thenCase), FrequentedBlock(elseCase));
    root->appendNew<Value>(proc, Branch, Origin(), condition);

    thenCase->clearSuccessors();
    thenCase->appendNew<Value>(proc, Return, Origin(), thenCase->appendNew<Const32Value>(proc, Origin(), 1));

    elseCase->clearSuccessors();
    elseCase->appendNew<Value>(proc, Return, Origin(), elseCase->appendNew<Const32Value>(proc, Origin(), 0));

    int32_t expected = (a == b && c == d) ? 1 : 0;
    CHECK_EQ(compileAndRun<int32_t>(proc, a, b, c, d), expected);
}

void testFCCmpOrDouble(double a, double b, double c, double d)
{
    // Test: (a == b) || (c == d)
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    BasicBlock* thenCase = proc.addBlock();
    BasicBlock* elseCase = proc.addBlock();
    auto arguments = cCallArgumentValues<double, double, double, double>(proc, root);

    Value* cmp1 = root->appendNew<Value>(proc, Equal, Origin(), arguments[0], arguments[1]);
    Value* cmp2 = root->appendNew<Value>(proc, Equal, Origin(), arguments[2], arguments[3]);
    Value* condition = root->appendNew<Value>(proc, BitOr, Origin(), cmp1, cmp2);

    root->setSuccessors(FrequentedBlock(thenCase), FrequentedBlock(elseCase));
    root->appendNew<Value>(proc, Branch, Origin(), condition);

    thenCase->clearSuccessors();
    thenCase->appendNew<Value>(proc, Return, Origin(), thenCase->appendNew<Const32Value>(proc, Origin(), 1));

    elseCase->clearSuccessors();
    elseCase->appendNew<Value>(proc, Return, Origin(), elseCase->appendNew<Const32Value>(proc, Origin(), 0));

    int32_t expected = (a == b || c == d) ? 1 : 0;
    CHECK_EQ(compileAndRun<int32_t>(proc, a, b, c, d), expected);
}

void testFCCmpAndFloat(float a, float b, float c, float d)
{
    // Test: (a == b) && (c == d)
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    BasicBlock* thenCase = proc.addBlock();
    BasicBlock* elseCase = proc.addBlock();
    auto arguments = cCallArgumentValues<float, float, float, float>(proc, root);

    Value* cmp1 = root->appendNew<Value>(proc, Equal, Origin(), arguments[0], arguments[1]);
    Value* cmp2 = root->appendNew<Value>(proc, Equal, Origin(), arguments[2], arguments[3]);
    Value* condition = root->appendNew<Value>(proc, BitAnd, Origin(), cmp1, cmp2);

    root->setSuccessors(FrequentedBlock(thenCase), FrequentedBlock(elseCase));
    root->appendNew<Value>(proc, Branch, Origin(), condition);

    thenCase->clearSuccessors();
    thenCase->appendNew<Value>(proc, Return, Origin(), thenCase->appendNew<Const32Value>(proc, Origin(), 1));

    elseCase->clearSuccessors();
    elseCase->appendNew<Value>(proc, Return, Origin(), elseCase->appendNew<Const32Value>(proc, Origin(), 0));

    int32_t expected = (a == b && c == d) ? 1 : 0;
    CHECK_EQ(compileAndRun<int32_t>(proc, a, b, c, d), expected);
}

void testFCCmpOrFloat(float a, float b, float c, float d)
{
    // Test: (a == b) || (c == d)
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    BasicBlock* thenCase = proc.addBlock();
    BasicBlock* elseCase = proc.addBlock();
    auto arguments = cCallArgumentValues<float, float, float, float>(proc, root);

    Value* cmp1 = root->appendNew<Value>(proc, Equal, Origin(), arguments[0], arguments[1]);
    Value* cmp2 = root->appendNew<Value>(proc, Equal, Origin(), arguments[2], arguments[3]);
    Value* condition = root->appendNew<Value>(proc, BitOr, Origin(), cmp1, cmp2);

    root->setSuccessors(FrequentedBlock(thenCase), FrequentedBlock(elseCase));
    root->appendNew<Value>(proc, Branch, Origin(), condition);

    thenCase->clearSuccessors();
    thenCase->appendNew<Value>(proc, Return, Origin(), thenCase->appendNew<Const32Value>(proc, Origin(), 1));

    elseCase->clearSuccessors();
    elseCase->appendNew<Value>(proc, Return, Origin(), elseCase->appendNew<Const32Value>(proc, Origin(), 0));

    int32_t expected = (a == b || c == d) ? 1 : 0;
    CHECK_EQ(compileAndRun<int32_t>(proc, a, b, c, d), expected);
}

void testFCCmpAndAndDouble(double a, double b, double c, double d, double e, double f)
{
    // Test: ((a == b) && (c == d)) && (e == f)
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    BasicBlock* thenCase = proc.addBlock();
    BasicBlock* elseCase = proc.addBlock();
    auto arguments = cCallArgumentValues<double, double, double, double, double, double>(proc, root);

    Value* cmp1 = root->appendNew<Value>(proc, Equal, Origin(), arguments[0], arguments[1]);
    Value* cmp2 = root->appendNew<Value>(proc, Equal, Origin(), arguments[2], arguments[3]);
    Value* and1 = root->appendNew<Value>(proc, BitAnd, Origin(), cmp1, cmp2);
    Value* cmp3 = root->appendNew<Value>(proc, Equal, Origin(), arguments[4], arguments[5]);
    Value* condition = root->appendNew<Value>(proc, BitAnd, Origin(), and1, cmp3);

    root->setSuccessors(FrequentedBlock(thenCase), FrequentedBlock(elseCase));
    root->appendNew<Value>(proc, Branch, Origin(), condition);

    thenCase->clearSuccessors();
    thenCase->appendNew<Value>(proc, Return, Origin(), thenCase->appendNew<Const32Value>(proc, Origin(), 1));

    elseCase->clearSuccessors();
    elseCase->appendNew<Value>(proc, Return, Origin(), elseCase->appendNew<Const32Value>(proc, Origin(), 0));

    int32_t expected = (a == b && c == d && e == f) ? 1 : 0;
    CHECK_EQ(compileAndRun<int32_t>(proc, a, b, c, d, e, f), expected);
}

void testFCCmpMixedIntDouble(int32_t a, int32_t b, double c, double d)
{
    // Test: (a == b) && (c < d) — int compare AND float compare
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    BasicBlock* thenCase = proc.addBlock();
    BasicBlock* elseCase = proc.addBlock();
    auto arguments = cCallArgumentValues<int32_t, int32_t, double, double>(proc, root);

    Value* cmp1 = root->appendNew<Value>(proc, Equal, Origin(), arguments[0], arguments[1]);
    Value* cmp2 = root->appendNew<Value>(proc, LessThan, Origin(), arguments[2], arguments[3]);
    Value* condition = root->appendNew<Value>(proc, BitAnd, Origin(), cmp1, cmp2);

    root->setSuccessors(FrequentedBlock(thenCase), FrequentedBlock(elseCase));
    root->appendNew<Value>(proc, Branch, Origin(), condition);

    thenCase->clearSuccessors();
    thenCase->appendNew<Value>(proc, Return, Origin(), thenCase->appendNew<Const32Value>(proc, Origin(), 1));

    elseCase->clearSuccessors();
    elseCase->appendNew<Value>(proc, Return, Origin(), elseCase->appendNew<Const32Value>(proc, Origin(), 0));

    int32_t expected = (a == b && c < d) ? 1 : 0;
    CHECK_EQ(compileAndRun<int32_t>(proc, a, b, c, d), expected);
}

void testFCCmpMixedDoubleInt(double a, double b, int32_t c, int32_t d)
{
    // Test: (a < b) && (c == d) — float compare AND int compare
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    BasicBlock* thenCase = proc.addBlock();
    BasicBlock* elseCase = proc.addBlock();
    auto arguments = cCallArgumentValues<double, double, int32_t, int32_t>(proc, root);

    Value* cmp1 = root->appendNew<Value>(proc, LessThan, Origin(), arguments[0], arguments[1]);
    Value* cmp2 = root->appendNew<Value>(proc, Equal, Origin(), arguments[2], arguments[3]);
    Value* condition = root->appendNew<Value>(proc, BitAnd, Origin(), cmp1, cmp2);

    root->setSuccessors(FrequentedBlock(thenCase), FrequentedBlock(elseCase));
    root->appendNew<Value>(proc, Branch, Origin(), condition);

    thenCase->clearSuccessors();
    thenCase->appendNew<Value>(proc, Return, Origin(), thenCase->appendNew<Const32Value>(proc, Origin(), 1));

    elseCase->clearSuccessors();
    elseCase->appendNew<Value>(proc, Return, Origin(), elseCase->appendNew<Const32Value>(proc, Origin(), 0));

    int32_t expected = (a < b && c == d) ? 1 : 0;
    CHECK_EQ(compileAndRun<int32_t>(proc, a, b, c, d), expected);
}

void testFCCmpLessThanAndDouble(double a, double b, double c, double d)
{
    // Test: (a < b) && (c < d)
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    BasicBlock* thenCase = proc.addBlock();
    BasicBlock* elseCase = proc.addBlock();
    auto arguments = cCallArgumentValues<double, double, double, double>(proc, root);

    Value* cmp1 = root->appendNew<Value>(proc, LessThan, Origin(), arguments[0], arguments[1]);
    Value* cmp2 = root->appendNew<Value>(proc, LessThan, Origin(), arguments[2], arguments[3]);
    Value* condition = root->appendNew<Value>(proc, BitAnd, Origin(), cmp1, cmp2);

    root->setSuccessors(FrequentedBlock(thenCase), FrequentedBlock(elseCase));
    root->appendNew<Value>(proc, Branch, Origin(), condition);

    thenCase->clearSuccessors();
    thenCase->appendNew<Value>(proc, Return, Origin(), thenCase->appendNew<Const32Value>(proc, Origin(), 1));

    elseCase->clearSuccessors();
    elseCase->appendNew<Value>(proc, Return, Origin(), elseCase->appendNew<Const32Value>(proc, Origin(), 0));

    int32_t expected = (a < b && c < d) ? 1 : 0;
    CHECK_EQ(compileAndRun<int32_t>(proc, a, b, c, d), expected);
}

void testFCCmpGreaterEqualOrDouble(double a, double b, double c, double d)
{
    // Test: (a >= b) || (c >= d)
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    BasicBlock* thenCase = proc.addBlock();
    BasicBlock* elseCase = proc.addBlock();
    auto arguments = cCallArgumentValues<double, double, double, double>(proc, root);

    Value* cmp1 = root->appendNew<Value>(proc, GreaterEqual, Origin(), arguments[0], arguments[1]);
    Value* cmp2 = root->appendNew<Value>(proc, GreaterEqual, Origin(), arguments[2], arguments[3]);
    Value* condition = root->appendNew<Value>(proc, BitOr, Origin(), cmp1, cmp2);

    root->setSuccessors(FrequentedBlock(thenCase), FrequentedBlock(elseCase));
    root->appendNew<Value>(proc, Branch, Origin(), condition);

    thenCase->clearSuccessors();
    thenCase->appendNew<Value>(proc, Return, Origin(), thenCase->appendNew<Const32Value>(proc, Origin(), 1));

    elseCase->clearSuccessors();
    elseCase->appendNew<Value>(proc, Return, Origin(), elseCase->appendNew<Const32Value>(proc, Origin(), 0));

    int32_t expected = (a >= b || c >= d) ? 1 : 0;
    CHECK_EQ(compileAndRun<int32_t>(proc, a, b, c, d), expected);
}

void testFCCmpNaN(double a, double b, double c, double d)
{
    // Test: (a == b) && (c == d) with NaN inputs
    // NaN comparisons should always be false
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    BasicBlock* thenCase = proc.addBlock();
    BasicBlock* elseCase = proc.addBlock();
    auto arguments = cCallArgumentValues<double, double, double, double>(proc, root);

    Value* cmp1 = root->appendNew<Value>(proc, Equal, Origin(), arguments[0], arguments[1]);
    Value* cmp2 = root->appendNew<Value>(proc, Equal, Origin(), arguments[2], arguments[3]);
    Value* condition = root->appendNew<Value>(proc, BitAnd, Origin(), cmp1, cmp2);

    root->setSuccessors(FrequentedBlock(thenCase), FrequentedBlock(elseCase));
    root->appendNew<Value>(proc, Branch, Origin(), condition);

    thenCase->clearSuccessors();
    thenCase->appendNew<Value>(proc, Return, Origin(), thenCase->appendNew<Const32Value>(proc, Origin(), 1));

    elseCase->clearSuccessors();
    elseCase->appendNew<Value>(proc, Return, Origin(), elseCase->appendNew<Const32Value>(proc, Origin(), 0));

    int32_t expected = (a == b && c == d) ? 1 : 0;
    CHECK_EQ(compileAndRun<int32_t>(proc, a, b, c, d), expected);
}

void testFCCmpNegatedAndDouble(double a, double b, double c, double d)
{
    // Test: !(a < b && c < d)
    // This becomes: (a < b && c < d) == 0
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    BasicBlock* thenCase = proc.addBlock();
    BasicBlock* elseCase = proc.addBlock();
    auto arguments = cCallArgumentValues<double, double, double, double>(proc, root);

    Value* cmp1 = root->appendNew<Value>(proc, LessThan, Origin(), arguments[0], arguments[1]);
    Value* cmp2 = root->appendNew<Value>(proc, LessThan, Origin(), arguments[2], arguments[3]);
    Value* andResult = root->appendNew<Value>(proc, BitAnd, Origin(), cmp1, cmp2);

    Value* negated = root->appendNew<Value>(
        proc, Equal, Origin(),
        andResult,
        root->appendNew<Const32Value>(proc, Origin(), 0));

    root->setSuccessors(FrequentedBlock(thenCase), FrequentedBlock(elseCase));
    root->appendNew<Value>(proc, Branch, Origin(), negated);

    thenCase->clearSuccessors();
    thenCase->appendNew<Value>(proc, Return, Origin(), thenCase->appendNew<Const32Value>(proc, Origin(), 1));

    elseCase->clearSuccessors();
    elseCase->appendNew<Value>(proc, Return, Origin(), elseCase->appendNew<Const32Value>(proc, Origin(), 0));

    int32_t expected = !(a < b && c < d) ? 1 : 0;
    CHECK_EQ(compileAndRun<int32_t>(proc, a, b, c, d), expected);
}

void testHeapRangeTreeNested()
{
    HeapRangeTree tree;
    tree.build({
        HeapRange(0, 100),
        HeapRange(0, 10),
        HeapRange(10, 20),
        HeapRange(20, 100),
        HeapRange(25, 30),
    });

    CHECK_EQ(tree.parent(tree.nodeFor(HeapRange(25, 30))), tree.nodeFor(HeapRange(20, 100)));
    CHECK_EQ(tree.depth(tree.nodeFor(HeapRange(0, 10))), 2u);
    CHECK_EQ(tree.nodeFor(HeapRange::top()), HeapRangeTree::root);
    CHECK_EQ(tree.nodeFor(HeapRange(200, 201)), HeapRangeTree::root);
}

void testHeapRangeTreePartialOverlapWrite()
{
    HeapRangeTree tree;
    tree.build({
        HeapRange(0, 10),
        HeapRange(10, 20),
    });

    CHECK_EQ(tree.nodeFor(HeapRange(5, 15)), HeapRangeTree::root);
    CHECK_EQ(tree.nodeFor(HeapRange(2, 3)), tree.nodeFor(HeapRange(0, 10)));
}

void testHeapRangeTreeCollapse()
{
    HeapRangeTree tree;
    tree.build({
        HeapRange(0, 100),
        HeapRange(0, 10),
        HeapRange(10, 20),
        HeapRange(20, 100),
        HeapRange(25, 30),
    });

    unsigned unchanged = tree.nodeFor(HeapRange(0, 100));

    tree.collapseToDepth(1);

    CHECK_EQ(tree.nodeFor(HeapRange(25, 30)), tree.nodeFor(HeapRange(20, 100)));
    CHECK_EQ(tree.nodeFor(HeapRange(0, 100)), unchanged);
}

void testHeapRangeTreeDuplicates()
{
    HeapRangeTree tree;
    tree.build({
        HeapRange(0, 10),
        HeapRange(0, 10),
        HeapRange(0, 10),
    });

    CHECK_EQ(tree.size(), 2u);
}

void testHeapRangeTreePartialOverlapNotANode()
{
    HeapRangeTree tree;
    tree.build({
        HeapRange(0, 100),
        HeapRange(0, 50),
        HeapRange(40, 60),
        HeapRange(45, 46),
    });

    CHECK_EQ(tree.nodeFor(HeapRange(40, 60)), tree.nodeFor(HeapRange(0, 100)));
    CHECK_EQ(tree.parent(tree.nodeFor(HeapRange(45, 46))), tree.nodeFor(HeapRange(0, 50)));
    CHECK_EQ(tree.nodeFor(HeapRange(42, 70)), tree.nodeFor(HeapRange(0, 100)));
    CHECK_EQ(tree.size(), 4u);
}

void testHeapRangeTreeOverlapImpliesAncestry()
{
    HeapRangeTree tree;
    Vector<HeapRange> buildRanges {
        HeapRange(0, 100),
        HeapRange(0, 50),
        HeapRange(40, 60),
        HeapRange(45, 46),
        HeapRange(50, 100),
        HeapRange(60, 70),
        HeapRange(65, 90),
        HeapRange(0, 10),
    };
    tree.build(Vector<HeapRange>(buildRanges));

    Vector<HeapRange> queries(buildRanges);
    queries.append(HeapRange(5, 15));
    queries.append(HeapRange(48, 52));
    queries.append(HeapRange(0, 1));
    queries.append(HeapRange(99, 100));
    queries.append(HeapRange(30, 80));
    queries.append(HeapRange::top());

    auto isAncestorOrSelf = [&](unsigned ancestor, unsigned node) {
        for (unsigned current = node; ; current = tree.parent(current)) {
            if (current == ancestor)
                return true;
            if (current == HeapRangeTree::root)
                return false;
        }
    };

    auto checkProperties = [&] {
        for (auto& x : queries) {
            unsigned nodeX = tree.nodeFor(x);
            HeapRange rangeX = tree.range(nodeX);
            CHECK(rangeX.begin() <= x.begin() && x.end() <= rangeX.end());
        }

        for (auto& x : queries) {
            for (auto& y : queries) {
                if (!x.overlaps(y))
                    continue;

                unsigned nodeX = tree.nodeFor(x);
                unsigned nodeY = tree.nodeFor(y);
                CHECK(isAncestorOrSelf(nodeX, nodeY) || isAncestorOrSelf(nodeY, nodeX));
            }
        }
    };

    checkProperties();

    tree.collapseToDepth(1);
    checkProperties();
}

void testHeapRangeTreeOverlapsAnyBuiltRange()
{
    HeapRangeTree empty;
    empty.build({ });
    CHECK(!empty.overlapsAnyBuiltRange(HeapRange(0, 10)));
    CHECK(!empty.overlapsAnyBuiltRange(HeapRange::top()));

    HeapRangeTree tree;
    tree.build({
        HeapRange(0, 10),
        HeapRange(2, 4),
        HeapRange(20, 30),
        HeapRange(25, 35),
    });

    auto check = [&] {
        CHECK(tree.overlapsAnyBuiltRange(HeapRange(3, 4)));
        CHECK(tree.overlapsAnyBuiltRange(HeapRange(21, 22)));
        CHECK(!tree.overlapsAnyBuiltRange(HeapRange(12, 18)));
        CHECK(!tree.overlapsAnyBuiltRange(HeapRange(10, 20)));
        CHECK(tree.overlapsAnyBuiltRange(HeapRange(8, 22)));
        CHECK(tree.overlapsAnyBuiltRange(HeapRange::top()));
        CHECK(!tree.overlapsAnyBuiltRange(HeapRange()));
        CHECK(!tree.overlapsAnyBuiltRange(HeapRange(40, 50)));
        // HeapRange(25, 35) is not a node, but a write to its uncovered part can still change it.
        CHECK(tree.overlapsAnyBuiltRange(HeapRange(32, 33)));
    };
    check();
    tree.collapseToDepth(0);
    check();

    HeapRangeTree everything;
    everything.build({ HeapRange::top() });
    CHECK(everything.overlapsAnyBuiltRange(HeapRange(100, 101)));
}

void testHeapRangeTreeCoversRange()
{
    HeapRangeTree empty;
    empty.build({ });
    CHECK(!empty.coversRange(HeapRange(0, 10)));
    CHECK(empty.coversRange(HeapRange()));

    HeapRangeTree tree;
    tree.build({
        HeapRange(0, 10),
        HeapRange(2, 4),
        HeapRange(10, 15),
        HeapRange(20, 30),
        HeapRange(25, 35),
    });

    auto check = [&] {
        CHECK(tree.coversRange(HeapRange(2, 4)));
        CHECK(tree.coversRange(HeapRange(0, 10)));
        // Adjacent built ranges cover a range that spans them.
        CHECK(tree.coversRange(HeapRange(5, 15)));
        CHECK(!tree.coversRange(HeapRange(5, 16)));
        CHECK(!tree.coversRange(HeapRange(14, 21)));
        // HeapRange(25, 35) is not a node, but it is covered.
        CHECK(tree.coversRange(HeapRange(21, 35)));
        CHECK(!tree.coversRange(HeapRange(21, 36)));
        CHECK(!tree.coversRange(HeapRange(40, 50)));
        CHECK(!tree.coversRange(HeapRange::top()));
        CHECK(tree.coversRange(HeapRange()));
    };
    check();
    tree.collapseToDepth(0);
    check();

    HeapRangeTree everything;
    everything.build({ HeapRange::top() });
    CHECK(everything.coversRange(HeapRange(100, 101)));
    CHECK(everything.coversRange(HeapRange::top()));
}

// The header stores through the pointer of the iteration it starts, so it never overwrites what
// the body stored through the previous iteration's pointer.
void testReduceStrengthDeadStoreAcrossBlocksKeptWhenBaseAdvances()
{
    if (Options::defaultB3OptLevel() < 2)
        return;

    Procedure proc;
    BasicBlock* root = proc.addBlock();
    BasicBlock* header = proc.addBlock();
    BasicBlock* body = proc.addBlock();
    BasicBlock* exit = proc.addBlock();

    Value* start = root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0);
    Value* end = root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR1);
    UpsilonValue* initial = root->appendNew<UpsilonValue>(proc, Origin(), start);
    root->setSuccessors(FrequentedBlock(header));
    root->appendNew<Value>(proc, Jump, Origin());

    Value* pointer = header->appendNew<Value>(proc, Phi, pointerType(), Origin());
    initial->setPhi(pointer);
    appendStoreWithRange(proc, header, Store, header->appendNew<Const32Value>(proc, Origin(), 1), pointer, HeapRange(10, 20));
    header->setSuccessors(FrequentedBlock(exit), FrequentedBlock(body));
    header->appendNew<Value>(proc, Branch, Origin(), header->appendNew<Value>(proc, Equal, Origin(), pointer, end));

    appendStoreWithRange(proc, body, Store, body->appendNew<Const32Value>(proc, Origin(), 2), pointer, HeapRange(10, 20));
    Value* next = body->appendNew<Value>(proc, Add, Origin(), pointer, body->appendNew<ConstPtrValue>(proc, Origin(), sizeof(int32_t)));
    body->appendNew<UpsilonValue>(proc, Origin(), next, pointer);
    body->setSuccessors(FrequentedBlock(header));
    body->appendNew<Value>(proc, Jump, Origin());

    exit->clearSuccessors();
    exit->appendNew<Value>(proc, Return, Origin());

    runReduceStrengthInitial(proc);

    CHECK_EQ(valuesStoredThrough(proc, pointer).size(), 2u);

    int32_t storage[4] = { };
    invoke<void>(*compileProc(proc), storage, storage + 3);
    CHECK_EQ(storage[0], 2);
    CHECK_EQ(storage[1], 2);
    CHECK_EQ(storage[2], 2);
    CHECK_EQ(storage[3], 1);
}

// Forwarding the root block's load inserts a mask constant that no visit numbers. A successor that
// peels an Identity down to the masking value must not leave it using a constant placed after it.
void testReduceStrengthIdentityReplacementKeepsRootOrder()
{
    if (Options::defaultB3OptLevel() < 2)
        return;

    Procedure proc;
    BasicBlock* root = proc.addBlock();
    BasicBlock* thenCase = proc.addBlock();
    BasicBlock* elseCase = proc.addBlock();

    Value* pointer = root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0);
    Value* stored = root->appendNew<Value>(proc, Trunc, Origin(), root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR1));
    Value* condition = root->appendNew<Value>(proc, Trunc, Origin(), root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR2));
    appendStoreWithRange(proc, root, Store8, stored, pointer, HeapRange(10, 20));
    MemoryValue* loaded = appendLoadWithRange(proc, root, Load8Z, Int32, pointer, HeapRange(10, 20));
    root->setSuccessors(FrequentedBlock(thenCase), FrequentedBlock(elseCase));
    root->appendNew<Value>(proc, Branch, Origin(), condition);

    thenCase->appendNew<Value>(proc, Return, Origin(), thenCase->appendNew<Value>(proc, BitOr, Origin(), loaded, thenCase->appendNew<Const32Value>(proc, Origin(), 0)));
    elseCase->appendNew<Value>(proc, Return, Origin(), loaded);

    runReduceStrengthInitial(proc);
    validate(proc);

    auto code = compileProc(proc);
    int64_t storage = 0;
    CHECK_EQ(invoke<int32_t>(*code, &storage, static_cast<int64_t>(0x1234), static_cast<int64_t>(1)), 0x34);
    CHECK_EQ(invoke<int32_t>(*code, &storage, static_cast<int64_t>(0x1256), static_cast<int64_t>(0)), 0x56);
}

#endif // ENABLE(B3_JIT)

WTF_ALLOW_UNSAFE_BUFFER_USAGE_END
