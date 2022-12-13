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

#include <wtf/UniqueArray.h>

#if ENABLE(B3_JIT) && !CPU(ARM)

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
            if (isARM64E())
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
    
        Value* ptr = root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0);
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
    
        Value* ptr = root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0);
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
    
        Value* ptr = root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0);
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
    
        Value* ptr = root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0);
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
        root->appendNew<Value>(
            proc, Return, Origin(),
            root->appendNew<AtomicValue>(
                proc, AtomicWeakCAS, Origin(), width,
                root->appendIntConstant(proc, Origin(), type, 42),
                root->appendIntConstant(proc, Origin(), type, 0xbeef),
                root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0)));
    
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
        root->appendNew<Value>(
            proc, Return, Origin(),
            root->appendNew<Value>(
                proc, Equal, Origin(),
                root->appendNew<AtomicValue>(
                    proc, AtomicWeakCAS, Origin(), width,
                    root->appendIntConstant(proc, Origin(), type, 42),
                    root->appendIntConstant(proc, Origin(), type, 0xbeef),
                    root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0)),
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
        root->appendNew<Value>(
            proc, Return, Origin(),
            root->appendNew<AtomicValue>(
                proc, AtomicWeakCAS, Origin(), width,
                root->appendIntConstant(proc, Origin(), type, 42),
                root->appendIntConstant(proc, Origin(), type, 0xbeef),
                root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0),
                42));
    
        auto code = compileProc(proc);
        T value[2];
        value[0] = 42;
        value[1] = 13;
        while (!invoke<bool>(*code, bitwise_cast<intptr_t>(value) - 42)) { }
        CHECK_EQ(value[0], static_cast<T>(0xbeef));
        CHECK_EQ(value[1], 13);
    
        value[0] = static_cast<T>(300);
        CHECK(!invoke<bool>(*code, bitwise_cast<intptr_t>(value) - 42));
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
            if (isARM64E())
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
    
        Value* ptr = root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0);
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
    
        Value* ptr = root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0);
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
    
        Value* ptr = root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0);
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
        root->appendNew<Value>(
            proc, Return, Origin(),
            root->appendNew<AtomicValue>(
                proc, AtomicStrongCAS, Origin(), width,
                root->appendIntConstant(proc, Origin(), type, 42),
                root->appendIntConstant(proc, Origin(), type, 0xbeef),
                root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0)));
    
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
        root->appendNew<Value>(
            proc, Return, Origin(),
            root->appendNew<Value>(
                proc, BitXor, Origin(),
                root->appendNew<AtomicValue>(
                    proc, AtomicStrongCAS, Origin(), width,
                    root->appendIntConstant(proc, Origin(), type, 42),
                    root->appendIntConstant(proc, Origin(), type, 0xbeef),
                    root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0)),
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
        root->appendNew<Value>(
            proc, Return, Origin(),
            root->appendNew<Value>(
                proc, Equal, Origin(),
                root->appendNew<AtomicValue>(
                    proc, AtomicStrongCAS, Origin(), width,
                    root->appendIntConstant(proc, Origin(), type, 42),
                    root->appendIntConstant(proc, Origin(), type, 0xbeef),
                    root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0)),
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
                        root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0)),
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

        Value* ptr = root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0);
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
            if (isARM64E()) {
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
        root->appendNew<Value>(
            proc, Return, Origin(),
            root->appendNew<AtomicValue>(
                proc, opcode, Origin(), width,
                root->appendIntConstant(proc, Origin(), type, 1),
                root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0)));

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
        root->appendNew<Value>(
            proc, Return, Origin(),
            root->appendNew<AtomicValue>(
                proc, opcode, Origin(), width,
                root->appendIntConstant(proc, Origin(), type, 42),
                root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0)));

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
        root->appendNew<AtomicValue>(
            proc, opcode, Origin(), width,
            root->appendIntConstant(proc, Origin(), type, 42),
            root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0));
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
        root->appendNew<AtomicValue>(
            proc, opcode, Origin(), width,
            root->appendIntConstant(proc, Origin(), type, 42),
            root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0),
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

void addAtomicTests(const char* filter, Deque<RefPtr<SharedTask<void()>>>& tasks)
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

        root->appendNewControlValue(
            proc, Return, Origin(),
            root->appendNew<MemoryValue>(
                proc, opcode, type, Origin(),
                root->appendNew<ConstPtrValue>(proc, Origin(), &value)));

        CHECK(isIdentical(compileAndRun<CType>(proc), modelLoad<CType>(value)));
    }

    // Simple load from an address in a register.
    {
        Procedure proc;
        BasicBlock* root = proc.addBlock();

        root->appendNewControlValue(
            proc, Return, Origin(),
            root->appendNew<MemoryValue>(
                proc, opcode, type, Origin(),
                root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0)));

        CHECK(isIdentical(compileAndRun<CType>(proc, &value), modelLoad<CType>(value)));
    }

    // Simple load from an address in a register, at an offset.
    {
        Procedure proc;
        BasicBlock* root = proc.addBlock();

        root->appendNewControlValue(
            proc, Return, Origin(),
            root->appendNew<MemoryValue>(
                proc, opcode, type, Origin(),
                root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0),
                static_cast<int32_t>(sizeof(InputType))));

        CHECK(isIdentical(compileAndRun<CType>(proc, &value - 1), modelLoad<CType>(value)));
    }

    // Load from a simple base-index with various scales.
    for (unsigned logScale = 0; logScale <= 3; ++logScale) {
        Procedure proc;
        BasicBlock* root = proc.addBlock();

        root->appendNewControlValue(
            proc, Return, Origin(),
            root->appendNew<MemoryValue>(
                proc, opcode, type, Origin(),
                root->appendNew<Value>(
                    proc, Add, Origin(),
                    root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0),
                    root->appendNew<Value>(
                        proc, Shl, Origin(),
                        root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR1),
                        root->appendNew<Const32Value>(proc, Origin(), logScale)))));

        CHECK(isIdentical(compileAndRun<CType>(proc, &value - 2, (sizeof(InputType) * 2) >> logScale), modelLoad<CType>(value)));
    }

    // Load from a simple base-index with various scales, but commuted.
    for (unsigned logScale = 0; logScale <= 3; ++logScale) {
        Procedure proc;
        BasicBlock* root = proc.addBlock();

        root->appendNewControlValue(
            proc, Return, Origin(),
            root->appendNew<MemoryValue>(
                proc, opcode, type, Origin(),
                root->appendNew<Value>(
                    proc, Add, Origin(),
                    root->appendNew<Value>(
                        proc, Shl, Origin(),
                        root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR1),
                        root->appendNew<Const32Value>(proc, Origin(), logScale)),
                    root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0))));

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

void addLoadTests(const char* filter, Deque<RefPtr<SharedTask<void()>>>& tasks)
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
        root->appendNew<WasmAddressValue>(proc, Origin(), root->appendNew<Const64Value>(proc, Origin(), 6*8), pinnedGPR), 0);

    SwitchValue* switchValue = root->appendNew<SwitchValue>(proc, Origin(), path);
    switchValue->setFallThrough(FrequentedBlock(c));
    switchValue->appendCase(SwitchCase(0, FrequentedBlock(a)));
    switchValue->appendCase(SwitchCase(1, FrequentedBlock(b)));

    PatchpointValue* patchpoint = b->appendNew<PatchpointValue>(proc, Void, Origin());
    patchpoint->effects = Effects::forCall();
    patchpoint->clobber(RegisterSetBuilder::macroClobberedRegisters());
    patchpoint->clobber(RegisterSetBuilder(pinnedGPR));
    patchpoint->setGenerator(
        [&] (CCallHelpers& jit, const StackmapGenerationParams& params) {
            CHECK(!params.size());
            jit.add64(MacroAssembler::TrustedImm32(8), pinnedGPR);
        });

    UpsilonValue* takeA = a->appendNew<UpsilonValue>(proc, Origin(), a->appendNew<Const32Value>(proc, Origin(), 10));
    UpsilonValue* takeB = b->appendNew<UpsilonValue>(proc, Origin(), b->appendNew<Const32Value>(proc, Origin(), 20));
    UpsilonValue* takeC = c->appendNew<UpsilonValue>(proc, Origin(), c->appendNew<Const32Value>(proc, Origin(), 30));
    for (auto* i : { a, b, c }) {
        i->appendNewControlValue(proc, Jump, Origin(), FrequentedBlock(continuation));
        i->setSuccessors(FrequentedBlock(continuation));
    }

    // Continuation
    auto* takenPhi = continuation->appendNew<Value>(proc, Phi, Int32, Origin());

    auto* address2 = continuation->appendNew<WasmAddressValue>(proc, Origin(), pointer, pinnedGPR);
    continuation->appendNew<MemoryValue>(proc, Store, Origin(), takenPhi,
        continuation->appendNew<WasmAddressValue>(proc, Origin(), continuation->appendNew<Const64Value>(proc, Origin(), 4*8), pinnedGPR),
        0);

    auto* returnVal = address2;
    continuation->appendNewControlValue(proc, Return, Origin(), returnVal);

    takeA->setPhi(takenPhi);
    takeB->setPhi(takenPhi);
    takeC->setPhi(takenPhi);

    auto binary = compileProc(proc);

    uint64_t* memory = new uint64_t[10];
    uint64_t ptr = 8;

    uint64_t finalPtr = reinterpret_cast<uint64_t>(static_cast<void*>(memory)) + ptr;

    for (int i = 0; i < 10; ++i)
        memory[i] = 0;

    {
        uint64_t result = invoke<uint64_t>(*binary, memory, ptr, 0);

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
        uint64_t result = invoke<uint64_t>(*binary, memory, ptr, 1);

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
        uint64_t result = invoke<uint64_t>(*binary, memory, ptr, 2);

        CHECK_EQ(result, finalPtr);
        CHECK_EQ(memory[0], 0ul);
        CHECK_EQ(memory[1], 0ul);
        CHECK_EQ(memory[2], 0ul);
        CHECK_EQ(memory[4], 30ul);
        CHECK_EQ(memory[6], finalPtr);
    }

    delete[] memory;
}

void testStoreAfterClobberExitsSideways()
{
    Procedure proc;
    GPRReg pinnedBaseGPR = GPRInfo::argumentGPR0;
    GPRReg pinnedSizeGPR = GPRInfo::argumentGPR1;
    proc.pinRegister(pinnedBaseGPR);
    proc.pinRegister(pinnedSizeGPR);

    // Please don't make me save anything.
    RegisterSetBuilder csrs;
    csrs.merge(RegisterSetBuilder::calleeSaveRegisters());
    csrs.exclude(RegisterSetBuilder::stackRegisters());
    csrs.buildAndValidate().forEach(
        [&] (Reg reg) {
            CHECK(reg != pinnedBaseGPR);
            CHECK(reg != pinnedSizeGPR);
            proc.pinRegister(reg);
        });

    proc.setWasmBoundsCheckGenerator([=] (CCallHelpers& jit, GPRReg pinnedGPR) {
        CHECK_EQ(pinnedGPR, pinnedSizeGPR);

        jit.move(CCallHelpers::TrustedImm32(42), GPRInfo::returnValueGPR);
        jit.emitFunctionEpilogue();
        jit.ret();
    });

    BasicBlock* root = proc.addBlock();

    auto* pointer = root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR2);
    auto* resultAddress = root->appendNew<WasmAddressValue>(proc, Origin(), pointer, pinnedBaseGPR);
    root->appendNew<MemoryValue>(proc, Store, Origin(), root->appendNew<Const32Value>(proc, Origin(), 10), resultAddress, 0);

    root->appendNew<WasmBoundsCheckValue>(proc, Origin(), pinnedSizeGPR, root->appendNew<Value>(proc, Trunc, Origin(), pointer), 0);

    root->appendNew<MemoryValue>(proc, Store, Origin(), root->appendNew<Const32Value>(proc, Origin(), 20), resultAddress, 0);
    root->appendNewControlValue(proc, Return, Origin(), root->appendNew<Const32Value>(proc, Origin(), 30));

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
    root->appendNew<MemoryValue>(proc, Store, Origin(), root->appendNew<Const64Value>(proc, Origin(), -1), resultAddress, 0);
    root->appendNew<MemoryValue>(proc, Store, Origin(), root->appendNew<Const32Value>(proc, Origin(), 20), resultAddress, 0);
    root->appendNewControlValue(proc, Return, Origin(), root->appendNew<Const32Value>(proc, Origin(), 30));

    auto binary = compileProc(proc);

    uint64_t* memory = new uint64_t[10];
    uint64_t ptr = 1*8;

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
    root->appendNew<MemoryValue>(proc, Store, Origin(), root->appendNew<Const64Value>(proc, Origin(), -1), resultAddress, 0);

    SwitchValue* switchValue = root->appendNew<SwitchValue>(proc, Origin(), path);
    switchValue->setFallThrough(FrequentedBlock(c));
    switchValue->appendCase(SwitchCase(0, FrequentedBlock(a)));
    switchValue->appendCase(SwitchCase(1, FrequentedBlock(b)));

    a->appendNew<MemoryValue>(proc, Store, Origin(), a->appendNew<Const32Value>(proc, Origin(), 10), resultAddress, 0);
    b->appendNew<MemoryValue>(proc, Store, Origin(), b->appendNew<Const32Value>(proc, Origin(), 20), resultAddress, 0);
    c->appendNew<MemoryValue>(proc, Store, Origin(), c->appendNew<Const32Value>(proc, Origin(), 30), resultAddress, 0);

    for (auto* i : { a, b, c }) {
        i->appendNewControlValue(proc, Jump, Origin(), FrequentedBlock(continuation));
        i->setSuccessors(FrequentedBlock(continuation));
    }

    continuation->appendNewControlValue(proc, Return, Origin(), continuation->appendNew<Const32Value>(proc, Origin(), 40));

    auto binary = compileProc(proc);

    uint64_t* memory = new uint64_t[10];
    uint64_t ptr = 1*8;

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
    RegisterSetBuilder csrs;
    csrs.merge(RegisterSetBuilder::calleeSaveRegisters());
    csrs.exclude(RegisterSetBuilder::stackRegisters());
    csrs.buildAndValidate().forEach(
        [&] (Reg reg) {
            CHECK(reg != pinnedBaseGPR);
            CHECK(reg != pinnedSizeGPR);
            proc.pinRegister(reg);
        });

    proc.setWasmBoundsCheckGenerator([=] (CCallHelpers& jit, GPRReg pinnedGPR) {
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

    auto* pointer = root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR2);
    auto* path = root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR3);
    auto* resultAddress = root->appendNew<WasmAddressValue>(proc, Origin(), pointer, pinnedBaseGPR);
    root->appendNew<MemoryValue>(proc, Store, Origin(), root->appendNew<Const64Value>(proc, Origin(), -1), resultAddress, 0);

    SwitchValue* switchValue = root->appendNew<SwitchValue>(proc, Origin(), path);
    switchValue->setFallThrough(FrequentedBlock(c));
    switchValue->appendCase(SwitchCase(0, FrequentedBlock(a)));
    switchValue->appendCase(SwitchCase(1, FrequentedBlock(b)));

    b->appendNew<WasmBoundsCheckValue>(proc, Origin(), pinnedSizeGPR, b->appendNew<Value>(proc, Trunc, Origin(), pointer), 0);

    UpsilonValue* takeA = a->appendNew<UpsilonValue>(proc, Origin(), a->appendNew<Const64Value>(proc, Origin(), 10));
    UpsilonValue* takeB = b->appendNew<UpsilonValue>(proc, Origin(), b->appendNew<Const64Value>(proc, Origin(), 20));
    UpsilonValue* takeC = c->appendNew<UpsilonValue>(proc, Origin(), c->appendNew<Const64Value>(proc, Origin(), 30));

    for (auto* i : { a, b, c }) {
        i->appendNewControlValue(proc, Jump, Origin(), FrequentedBlock(continuation));
        i->setSuccessors(FrequentedBlock(continuation));
    }

    auto* takenPhi = continuation->appendNew<Value>(proc, Phi, Int64, Origin());
    continuation->appendNew<MemoryValue>(proc, Store, Origin(), takenPhi, resultAddress, 0);
    continuation->appendNewControlValue(proc, Return, Origin(), continuation->appendNew<Const32Value>(proc, Origin(), 40));

    takeA->setPhi(takenPhi);
    takeB->setPhi(takenPhi);
    takeC->setPhi(takenPhi);

    auto binary = compileProc(proc);

    uint64_t* memory = new uint64_t[10];
    uint64_t ptr = 1*8;

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

#endif // ENABLE(B3_JIT)
