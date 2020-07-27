/*
 * Copyright (C) 2020 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include <JavaScriptCore/InitializeThreading.h>
#include <JavaScriptCore/JSCJSValueInlines.h>
#include <JavaScriptCore/PropertySlot.h>
#include <JavaScriptCore/VM.h>

namespace TestWebKitAPI {

using JSC::JSLockHolder;
using JSC::LargeHeap;
using JSC::PropertySlot;
using JSC::VM;
using JSC::jsUndefined;

TEST(JavaScriptCore_PropertySlot, DisallowVMEntryCountBasic)
{
    WTF::initializeMainThread();
    JSC::initialize();

    VM& vm = VM::create(LargeHeap).leakRef();
    {
        JSLockHolder locker(vm);
        EXPECT_EQ(vm.disallowVMEntryCount, 0u);
        {
            PropertySlot slot(jsUndefined(), PropertySlot::InternalMethodType::Get);
            EXPECT_FALSE(!!slot.disallowVMEntry);
            EXPECT_EQ(vm.disallowVMEntryCount, 0u);
        }
        EXPECT_EQ(vm.disallowVMEntryCount, 0u);
        {
            PropertySlot slot(jsUndefined(), PropertySlot::InternalMethodType::HasProperty);
            EXPECT_FALSE(!!slot.disallowVMEntry);
            EXPECT_EQ(vm.disallowVMEntryCount, 0u);
        }
        EXPECT_EQ(vm.disallowVMEntryCount, 0u);
        {
            PropertySlot slot(jsUndefined(), PropertySlot::InternalMethodType::GetOwnProperty);
            EXPECT_FALSE(!!slot.disallowVMEntry);
            EXPECT_EQ(vm.disallowVMEntryCount, 0u);
        }
        EXPECT_EQ(vm.disallowVMEntryCount, 0u);
        {
            PropertySlot slot(jsUndefined(), PropertySlot::InternalMethodType::VMInquiry, &vm);
            EXPECT_TRUE(!!slot.disallowVMEntry);
            EXPECT_EQ(vm.disallowVMEntryCount, 1u);
        }
        EXPECT_EQ(vm.disallowVMEntryCount, 0u);
    }
}

TEST(JavaScriptCore_PropertySlot, VMInquiryReset)
{
    WTF::initializeMainThread();
    JSC::initialize();

    VM& vm = VM::create(LargeHeap).leakRef();
    {
        JSLockHolder locker(vm);
        EXPECT_EQ(vm.disallowVMEntryCount, 0u);

        {
            PropertySlot slot(jsUndefined(), PropertySlot::InternalMethodType::VMInquiry, &vm);
            EXPECT_TRUE(!!slot.disallowVMEntry);
            EXPECT_EQ(vm.disallowVMEntryCount, 1u);

            slot.disallowVMEntry.reset();
            EXPECT_FALSE(!!slot.disallowVMEntry);
            EXPECT_EQ(vm.disallowVMEntryCount, 0u);
        }

        EXPECT_EQ(vm.disallowVMEntryCount, 0u);
    }
}

TEST(JavaScriptCore_PropertySlot, CopyAssignment)
{
    WTF::initializeMainThread();
    JSC::initialize();

    VM& vm = VM::create(LargeHeap).leakRef();
    {
        JSLockHolder locker(vm);
        EXPECT_EQ(vm.disallowVMEntryCount, 0u);

        {
            PropertySlot slot(jsUndefined(), PropertySlot::InternalMethodType::Get);
            EXPECT_FALSE(slot.isVMInquiry());
            EXPECT_FALSE(!!slot.disallowVMEntry);
            EXPECT_EQ(vm.disallowVMEntryCount, 0u);

            {
                PropertySlot slot2 = slot;
                EXPECT_FALSE(slot.isVMInquiry());
                EXPECT_FALSE(!!slot.disallowVMEntry);
                EXPECT_EQ(vm.disallowVMEntryCount, 0u);

                slot = slot2;
                EXPECT_FALSE(slot.isVMInquiry());
                EXPECT_FALSE(!!slot.disallowVMEntry);
                EXPECT_EQ(vm.disallowVMEntryCount, 0u);
            }

            EXPECT_FALSE(slot.isVMInquiry());
            EXPECT_FALSE(!!slot.disallowVMEntry);
            EXPECT_EQ(vm.disallowVMEntryCount, 0u);
        }

        EXPECT_EQ(vm.disallowVMEntryCount, 0u);

        {
            PropertySlot slot(jsUndefined(), PropertySlot::InternalMethodType::VMInquiry, &vm);
            EXPECT_TRUE(slot.isVMInquiry());
            EXPECT_TRUE(!!slot.disallowVMEntry);
            EXPECT_EQ(vm.disallowVMEntryCount, 1u);

            {
                PropertySlot slot2 = slot;
                EXPECT_TRUE(slot2.isVMInquiry());
                EXPECT_TRUE(!!slot2.disallowVMEntry);
                EXPECT_EQ(vm.disallowVMEntryCount, 2u);

                slot = slot2;
                EXPECT_TRUE(slot.isVMInquiry());
                EXPECT_TRUE(!!slot.disallowVMEntry);
                EXPECT_EQ(vm.disallowVMEntryCount, 2u);
            }

            EXPECT_TRUE(slot.isVMInquiry());
            EXPECT_TRUE(!!slot.disallowVMEntry);
            EXPECT_EQ(vm.disallowVMEntryCount, 1u);
        }

        EXPECT_EQ(vm.disallowVMEntryCount, 0u);
    }
}

} // namespace TestWebKitAPI
