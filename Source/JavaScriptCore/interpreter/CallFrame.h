/*
 *  Copyright (C) 1999-2001 Harri Porten (porten@kde.org)
 *  Copyright (C) 2001 Peter Kelly (pmk@post.com)
 *  Copyright (C) 2003, 2007, 2008, 2011, 2013, 2014 Apple Inc. All rights reserved.
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Library General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Library General Public License for more details.
 *
 *  You should have received a copy of the GNU Library General Public License
 *  along with this library; see the file COPYING.LIB.  If not, write to
 *  the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 *  Boston, MA 02110-1301, USA.
 *
 */

#ifndef CallFrame_h
#define CallFrame_h

#include "AbstractPC.h"
#include "VM.h"
#include "JSStack.h"
#include "MacroAssemblerCodeRef.h"
#include "Register.h"
#include "StackVisitor.h"

namespace JSC  {

    class Arguments;
    class JSActivation;
    class Interpreter;
    class JSScope;

    // Represents the current state of script execution.
    // Passed as the first argument to most functions.
    class ExecState : private Register {
    public:
        JSValue calleeAsValue() const { return this[JSStack::Callee].jsValue(); }
        JSObject* callee() const { return this[JSStack::Callee].function(); }
        CodeBlock* codeBlock() const { return this[JSStack::CodeBlock].Register::codeBlock(); }
        JSScope* scope() const
        {
            ASSERT(this[JSStack::ScopeChain].Register::scope());
            return this[JSStack::ScopeChain].Register::scope();
        }

        bool hasActivation() const { return !!uncheckedActivation(); }
        JSActivation* activation() const;
        JSValue uncheckedActivation() const;

        // Global object in which execution began.
        JS_EXPORT_PRIVATE JSGlobalObject* vmEntryGlobalObject();

        // Global object in which the currently executing code was defined.
        // Differs from vmEntryGlobalObject() during function calls across web browser frames.
        JSGlobalObject* lexicalGlobalObject() const;

        // Differs from lexicalGlobalObject because this will have DOM window shell rather than
        // the actual DOM window, which can't be "this" for security reasons.
        JSObject* globalThisValue() const;

        VM& vm() const;

        // Convenience functions for access to global data.
        // It takes a few memory references to get from a call frame to the global data
        // pointer, so these are inefficient, and should be used sparingly in new code.
        // But they're used in many places in legacy code, so they're not going away any time soon.

        void clearException() { vm().clearException(); }
        void clearSupplementaryExceptionInfo()
        {
            vm().clearExceptionStack();
        }

        JSValue exception() const { return vm().exception(); }
        bool hadException() const { return !vm().exception().isEmpty(); }

        const CommonIdentifiers& propertyNames() const { return *vm().propertyNames; }
        const MarkedArgumentBuffer& emptyList() const { return *vm().emptyList; }
        Interpreter* interpreter() { return vm().interpreter; }
        Heap* heap() { return &vm().heap; }

        static const HashTable& arrayConstructorTable(VM& vm) { return *vm.arrayConstructorTable; }
        static const HashTable& arrayPrototypeTable(VM& vm) { return *vm.arrayPrototypeTable; }
        static const HashTable& booleanPrototypeTable(VM& vm) { return *vm.booleanPrototypeTable; }
        static const HashTable& dataViewTable(VM& vm) { return *vm.dataViewTable; }
        static const HashTable& dateTable(VM& vm) { return *vm.dateTable; }
        static const HashTable& dateConstructorTable(VM& vm) { return *vm.dateConstructorTable; }
        static const HashTable& errorPrototypeTable(VM& vm) { return *vm.errorPrototypeTable; }
        static const HashTable& globalObjectTable(VM& vm) { return *vm.globalObjectTable; }
        static const HashTable& jsonTable(VM& vm) { return *vm.jsonTable; }
        static const HashTable& numberConstructorTable(VM& vm) { return *vm.numberConstructorTable; }
        static const HashTable& numberPrototypeTable(VM& vm) { return *vm.numberPrototypeTable; }
        static const HashTable& objectConstructorTable(VM& vm) { return *vm.objectConstructorTable; }
        static const HashTable& privateNamePrototypeTable(VM& vm) { return *vm.privateNamePrototypeTable; }
        static const HashTable& regExpTable(VM& vm) { return *vm.regExpTable; }
        static const HashTable& regExpConstructorTable(VM& vm) { return *vm.regExpConstructorTable; }
        static const HashTable& regExpPrototypeTable(VM& vm) { return *vm.regExpPrototypeTable; }
        static const HashTable& stringConstructorTable(VM& vm) { return *vm.stringConstructorTable; }
#if ENABLE(PROMISES)
        static const HashTable& promisePrototypeTable(VM& vm) { return *vm.promisePrototypeTable; }
        static const HashTable& promiseConstructorTable(VM& vm) { return *vm.promiseConstructorTable; }
#endif

        static CallFrame* create(Register* callFrameBase) { return static_cast<CallFrame*>(callFrameBase); }
        Register* registers() { return this; }
        const Register* registers() const { return this; }

        CallFrame& operator=(const Register& r) { *static_cast<Register*>(this) = r; return *this; }

        CallFrame* callerFrame() const { return callerFrameAndPC().callerFrame; }
        static ptrdiff_t callerFrameOffset() { return OBJECT_OFFSETOF(CallerFrameAndPC, callerFrame); }

        ReturnAddressPtr returnPC() const { return ReturnAddressPtr(callerFrameAndPC().pc); }
        bool hasReturnPC() const { return !!callerFrameAndPC().pc; }
        void clearReturnPC() { callerFrameAndPC().pc = 0; }
        static ptrdiff_t returnPCOffset() { return OBJECT_OFFSETOF(CallerFrameAndPC, pc); }
        AbstractPC abstractReturnPC(VM& vm) { return AbstractPC(vm, this); }

        class Location {
        public:
            static inline uint32_t decode(uint32_t bits);

            static inline bool isBytecodeLocation(uint32_t bits);
#if USE(JSVALUE64)
            static inline uint32_t encodeAsBytecodeOffset(uint32_t bits);
#else
            static inline uint32_t encodeAsBytecodeInstruction(Instruction*);
#endif

            static inline bool isCodeOriginIndex(uint32_t bits);
            static inline uint32_t encodeAsCodeOriginIndex(uint32_t bits);

        private:
            enum TypeTag {
                BytecodeLocationTag = 0,
                CodeOriginIndexTag = 1,
            };

            static inline uint32_t encode(TypeTag, uint32_t bits);

            static const uint32_t s_mask = 0x1;
#if USE(JSVALUE64)
            static const uint32_t s_shift = 31;
            static const uint32_t s_shiftedMask = s_mask << s_shift;
#else
            static const uint32_t s_shift = 1;
#endif
        };

        bool hasLocationAsBytecodeOffset() const;
        bool hasLocationAsCodeOriginIndex() const;

        unsigned locationAsRawBits() const;
        unsigned locationAsBytecodeOffset() const;
        unsigned locationAsCodeOriginIndex() const;

        void setLocationAsRawBits(unsigned);
        void setLocationAsBytecodeOffset(unsigned);

#if ENABLE(DFG_JIT)
        unsigned bytecodeOffsetFromCodeOriginIndex();
#endif
        
        // This will try to get you the bytecode offset, but you should be aware that
        // this bytecode offset may be bogus in the presence of inlining. This will
        // also return 0 if the call frame has no notion of bytecode offsets (for
        // example if it's native code).
        // https://bugs.webkit.org/show_bug.cgi?id=121754
        unsigned bytecodeOffset();
        
        // This will get you a CodeOrigin. It will always succeed. May return
        // CodeOrigin(0) if we're in native code.
        CodeOrigin codeOrigin();

        Register* topOfFrame()
        {
            if (isVMEntrySentinel() || !codeBlock())
                return registers();
            return topOfFrameInternal();
        }
    
#if USE(JSVALUE32_64)
        Instruction* currentVPC() const
        {
            ASSERT(!isVMEntrySentinel());
            return bitwise_cast<Instruction*>(this[JSStack::ArgumentCount].tag());
        }
        void setCurrentVPC(Instruction* vpc)
        {
            ASSERT(!isVMEntrySentinel());
            this[JSStack::ArgumentCount].tag() = bitwise_cast<int32_t>(vpc);
        }
#else
        Instruction* currentVPC() const;
        void setCurrentVPC(Instruction* vpc);
#endif

        void setCallerFrame(CallFrame* frame) { callerFrameAndPC().callerFrame = frame; }
        void setScope(JSScope* scope) { static_cast<Register*>(this)[JSStack::ScopeChain] = scope; }
        void setActivation(JSActivation*);

        ALWAYS_INLINE void init(CodeBlock* codeBlock, Instruction* vPC, JSScope* scope,
            CallFrame* callerFrame, int argc, JSObject* callee)
        {
            ASSERT(callerFrame == noCaller() || callerFrame->isVMEntrySentinel() || callerFrame->stack()->containsAddress(this));

            setCodeBlock(codeBlock);
            setScope(scope);
            setCallerFrame(callerFrame);
            setReturnPC(vPC); // This is either an Instruction* or a pointer into JIT generated code stored as an Instruction*.
            setArgumentCountIncludingThis(argc); // original argument count (for the sake of the "arguments" object)
            setCallee(callee);
        }

        // Read a register from the codeframe (or constant from the CodeBlock).
        Register& r(int);
        // Read a register for a non-constant
        Register& uncheckedR(int);

        // Access to arguments as passed. (After capture, arguments may move to a different location.)
        size_t argumentCount() const { return argumentCountIncludingThis() - 1; }
        size_t argumentCountIncludingThis() const { return this[JSStack::ArgumentCount].payload(); }
        static int argumentOffset(int argument) { return (JSStack::FirstArgument + argument); }
        static int argumentOffsetIncludingThis(int argument) { return (JSStack::ThisArgument + argument); }

        // In the following (argument() and setArgument()), the 'argument'
        // parameter is the index of the arguments of the target function of
        // this frame. The index starts at 0 for the first arg, 1 for the
        // second, etc.
        //
        // The arguments (in this case) do not include the 'this' value.
        // arguments(0) will not fetch the 'this' value. To get/set 'this',
        // use thisValue() and setThisValue() below.

        JSValue argument(size_t argument)
        {
            if (argument >= argumentCount())
                 return jsUndefined();
            return getArgumentUnsafe(argument);
        }
        JSValue uncheckedArgument(size_t argument)
        {
            ASSERT(argument < argumentCount());
            return getArgumentUnsafe(argument);
        }
        void setArgument(size_t argument, JSValue value)
        {
            this[argumentOffset(argument)] = value;
        }

        static int thisArgumentOffset() { return argumentOffsetIncludingThis(0); }
        JSValue thisValue() { return this[thisArgumentOffset()].jsValue(); }
        void setThisValue(JSValue value) { this[thisArgumentOffset()] = value; }

        JSValue argumentAfterCapture(size_t argument);

        static int offsetFor(size_t argumentCountIncludingThis) { return argumentCountIncludingThis + JSStack::ThisArgument - 1; }

        // FIXME: Remove these.
        int hostThisRegister() { return thisArgumentOffset(); }
        JSValue hostThisValue() { return thisValue(); }

        static CallFrame* noCaller() { return 0; }

        bool isVMEntrySentinel() const
        {
            return !!this && codeBlock() == vmEntrySentinelCodeBlock();
        }

        CallFrame* vmEntrySentinelCallerFrame() const
        {
            ASSERT(isVMEntrySentinel());
            return this[JSStack::ScopeChain].callFrame();
        }

        void initializeVMEntrySentinelFrame(CallFrame* callFrame)
        {
            setCallerFrame(noCaller());
            setReturnPC(0);
            setCodeBlock(vmEntrySentinelCodeBlock());
            static_cast<Register*>(this)[JSStack::ScopeChain] = callFrame;
            setCallee(0);
            setArgumentCountIncludingThis(0);
        }

        CallFrame* callerFrameSkippingVMEntrySentinel()
        {
            CallFrame* caller = callerFrame();
            if (caller->isVMEntrySentinel())
                return caller->vmEntrySentinelCallerFrame();
            return caller;
        }

        void setArgumentCountIncludingThis(int count) { static_cast<Register*>(this)[JSStack::ArgumentCount].payload() = count; }
        void setCallee(JSObject* callee) { static_cast<Register*>(this)[JSStack::Callee] = Register::withCallee(callee); }
        void setCodeBlock(CodeBlock* codeBlock) { static_cast<Register*>(this)[JSStack::CodeBlock] = codeBlock; }
        void setReturnPC(void* value) { callerFrameAndPC().pc = reinterpret_cast<Instruction*>(value); }

        // CallFrame::iterate() expects a Functor that implements the following method:
        //     StackVisitor::Status operator()(StackVisitor&);

        template <typename Functor> void iterate(Functor& functor)
        {
            StackVisitor::visit<Functor>(this, functor);
        }

    private:
        static const intptr_t s_VMEntrySentinel = 1;

#ifndef NDEBUG
        JSStack* stack();
#endif
        ExecState();
        ~ExecState();

        Register* topOfFrameInternal();

        // The following are for internal use in debugging and verification
        // code only and not meant as an API for general usage:

        size_t argIndexForRegister(Register* reg)
        {
            // The register at 'offset' number of slots from the frame pointer
            // i.e.
            //       reg = frame[offset];
            //   ==> reg = frame + offset;
            //   ==> offset = reg - frame;
            int offset = reg - this->registers();

            // The offset is defined (based on argumentOffset()) to be:
            //       offset = JSStack::FirstArgument - argIndex;
            // Hence:
            //       argIndex = JSStack::FirstArgument - offset;
            size_t argIndex = offset - JSStack::FirstArgument;
            return argIndex;
        }

        JSValue getArgumentUnsafe(size_t argIndex)
        {
            // User beware! This method does not verify that there is a valid
            // argument at the specified argIndex. This is used for debugging
            // and verification code only. The caller is expected to know what
            // he/she is doing when calling this method.
            return this[argumentOffset(argIndex)].jsValue();
        }

        CallerFrameAndPC& callerFrameAndPC() { return *reinterpret_cast<CallerFrameAndPC*>(this); }
        const CallerFrameAndPC& callerFrameAndPC() const { return *reinterpret_cast<const CallerFrameAndPC*>(this); }

        static CodeBlock* vmEntrySentinelCodeBlock() { return reinterpret_cast<CodeBlock*>(s_VMEntrySentinel); }

        friend class JSStack;
        friend class VMInspector;
    };

} // namespace JSC

#endif // CallFrame_h
