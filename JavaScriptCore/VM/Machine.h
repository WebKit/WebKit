/*
 * Copyright (C) 2008 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef Machine_h
#define Machine_h

#include "ArgList.h"
#include "JSCell.h"
#include "JSValue.h"
#include "Opcode.h"
#include "RegisterFile.h"
#include <wtf/HashMap.h>

namespace JSC {

    class CodeBlock;
    class EvalNode;
    class FunctionBodyNode;
    class Instruction;
    class InternalFunction;
    class JITCodeBuffer;
    class JSFunction;
    class JSGlobalObject;
    class ProgramNode;
    class Register;
    class ScopeChainNode;
    class SamplingTool;

#if ENABLE(CTI)

#if COMPILER(MSVC)
#define SFX_CALL __fastcall
#elif COMPILER(GCC)
#define SFX_CALL  __attribute__ ((fastcall))
#else
#error Need to support fastcall calling convention in this compiler
#endif

    struct VoidPtrPair { void* first; void* second; };
#endif

    enum DebugHookID {
        WillExecuteProgram,
        DidExecuteProgram,
        DidEnterCallFrame,
        DidReachBreakpoint,
        WillLeaveCallFrame,
        WillExecuteStatement
    };

    enum { MaxReentryDepth = 128 };

    class Machine {
        friend class CTI;
    public:
        Machine();
        ~Machine();
        
        RegisterFile& registerFile() { return m_registerFile; }
        
        Opcode getOpcode(OpcodeID id)
        {
            #if HAVE(COMPUTED_GOTO)
                return m_opcodeTable[id];
            #else
                return id;
            #endif
        }

        OpcodeID getOpcodeID(Opcode opcode)
        {
            #if HAVE(COMPUTED_GOTO)
                ASSERT(isOpcode(opcode));
                return m_opcodeIDTable.get(opcode);
            #else
                return opcode;
            #endif
        }

        bool isOpcode(Opcode opcode);
        
        JSValue* execute(ProgramNode*, CallFrame*, ScopeChainNode*, JSObject* thisObj, JSValue** exception);
        JSValue* execute(FunctionBodyNode*, CallFrame*, JSFunction*, JSObject* thisObj, const ArgList& args, ScopeChainNode*, JSValue** exception);
        JSValue* execute(EvalNode* evalNode, CallFrame* exec, JSObject* thisObj, ScopeChainNode* scopeChain, JSValue** exception);

        JSValue* retrieveArguments(CallFrame*, JSFunction*) const;
        JSValue* retrieveCaller(CallFrame*, InternalFunction*) const;
        void retrieveLastCaller(CallFrame*, int& lineNumber, intptr_t& sourceID, UString& sourceURL, JSValue*& function) const;
        
        void getArgumentsData(CallFrame*, JSFunction*&, ptrdiff_t& firstParameterIndex, Register*& argv, int& argc);
        void setTimeoutTime(unsigned timeoutTime) { m_timeoutTime = timeoutTime; }
        
        void startTimeoutCheck()
        {
            if (!m_timeoutCheckCount)
                resetTimeoutCheck();
            
            ++m_timeoutCheckCount;
        }
        
        void stopTimeoutCheck()
        {
            ASSERT(m_timeoutCheckCount);
            --m_timeoutCheckCount;
        }

        inline void initTimeout()
        {
            ASSERT(!m_timeoutCheckCount);
            resetTimeoutCheck();
            m_timeoutTime = 0;
            m_timeoutCheckCount = 0;
        }

        SamplingTool* m_sampler;

#if ENABLE(CTI)

        static void SFX_CALL cti_timeout_check(void** args);
        static void SFX_CALL cti_register_file_check(void** args);

        static JSValue* SFX_CALL cti_op_convert_this(void** args);
        static void SFX_CALL cti_op_end(void** args);
        static JSValue* SFX_CALL cti_op_add(void** args);
        static JSValue* SFX_CALL cti_op_pre_inc(void** args);
        static int SFX_CALL cti_op_loop_if_less(void** args);
        static int SFX_CALL cti_op_loop_if_lesseq(void** args);
        static JSValue* SFX_CALL cti_op_new_object(void** args);
        static void SFX_CALL cti_op_put_by_id(void** args);
        static void SFX_CALL cti_op_put_by_id_second(void** args);
        static void SFX_CALL cti_op_put_by_id_generic(void** args);
        static void SFX_CALL cti_op_put_by_id_fail(void** args);
        static JSValue* SFX_CALL cti_op_get_by_id(void** args);
        static JSValue* SFX_CALL cti_op_get_by_id_second(void** args);
        static JSValue* SFX_CALL cti_op_get_by_id_generic(void** args);
        static JSValue* SFX_CALL cti_op_get_by_id_fail(void** args);
        static JSValue* SFX_CALL cti_op_del_by_id(void** args);
        static JSValue* SFX_CALL cti_op_instanceof(void** args);
        static JSValue* SFX_CALL cti_op_mul(void** args);
        static JSValue* SFX_CALL cti_op_new_func(void** args);
        static VoidPtrPair SFX_CALL cti_op_call_JSFunction(void** args);
        static JSValue* SFX_CALL cti_op_call_NotJSFunction(void** args);
        static void SFX_CALL cti_op_create_arguments(void** args);
        static void SFX_CALL cti_op_tear_off_activation(void** args);
        static void SFX_CALL cti_op_tear_off_arguments(void** args);
        static void SFX_CALL cti_op_ret_profiler(void** args);
        static void SFX_CALL cti_op_ret_scopeChain(void** args);
        static JSValue* SFX_CALL cti_op_new_array(void** args);
        static JSValue* SFX_CALL cti_op_resolve(void** args);
        static JSValue* SFX_CALL cti_op_resolve_global(void** args);
        static VoidPtrPair SFX_CALL cti_op_construct_JSConstruct(void** args);
        static JSValue* SFX_CALL cti_op_construct_NotJSConstruct(void** args);
        static JSValue* SFX_CALL cti_op_get_by_val(void** args);
        static VoidPtrPair SFX_CALL cti_op_resolve_func(void** args);
        static JSValue* SFX_CALL cti_op_sub(void** args);
        static void SFX_CALL cti_op_put_by_val(void** args);
        static void SFX_CALL cti_op_put_by_val_array(void** args);
        static JSValue* SFX_CALL cti_op_lesseq(void** args);
        static int SFX_CALL cti_op_loop_if_true(void** args);
        static JSValue* SFX_CALL cti_op_resolve_base(void** args);
        static JSValue* SFX_CALL cti_op_negate(void** args);
        static JSValue* SFX_CALL cti_op_resolve_skip(void** args);
        static JSValue* SFX_CALL cti_op_div(void** args);
        static JSValue* SFX_CALL cti_op_pre_dec(void** args);
        static int SFX_CALL cti_op_jless(void** args);
        static JSValue* SFX_CALL cti_op_not(void** args);
        static int SFX_CALL cti_op_jtrue(void** args);
        static VoidPtrPair SFX_CALL cti_op_post_inc(void** args);
        static JSValue* SFX_CALL cti_op_eq(void** args);
        static JSValue* SFX_CALL cti_op_lshift(void** args);
        static JSValue* SFX_CALL cti_op_bitand(void** args);
        static JSValue* SFX_CALL cti_op_rshift(void** args);
        static JSValue* SFX_CALL cti_op_bitnot(void** args);
        static VoidPtrPair SFX_CALL cti_op_resolve_with_base(void** args);
        static JSValue* SFX_CALL cti_op_new_func_exp(void** args);
        static JSValue* SFX_CALL cti_op_mod(void** args);
        static JSValue* SFX_CALL cti_op_less(void** args);
        static JSValue* SFX_CALL cti_op_neq(void** args);
        static VoidPtrPair SFX_CALL cti_op_post_dec(void** args);
        static JSValue* SFX_CALL cti_op_urshift(void** args);
        static JSValue* SFX_CALL cti_op_bitxor(void** args);
        static JSValue* SFX_CALL cti_op_new_regexp(void** args);
        static JSValue* SFX_CALL cti_op_bitor(void** args);
        static JSValue* SFX_CALL cti_op_call_eval(void** args);
        static void* SFX_CALL cti_op_throw(void** args);
        static JSPropertyNameIterator* SFX_CALL cti_op_get_pnames(void** args);
        static JSValue* SFX_CALL cti_op_next_pname(void** args);
        static void SFX_CALL cti_op_push_scope(void** args);
        static void SFX_CALL cti_op_pop_scope(void** args);
        static JSValue* SFX_CALL cti_op_typeof(void** args);
        static JSValue* SFX_CALL cti_op_is_undefined(void** args);
        static JSValue* SFX_CALL cti_op_is_boolean(void** args);
        static JSValue* SFX_CALL cti_op_is_number(void** args);
        static JSValue* SFX_CALL cti_op_is_string(void** args);
        static JSValue* SFX_CALL cti_op_is_object(void** args);
        static JSValue* SFX_CALL cti_op_is_function(void** args);
        static JSValue* SFX_CALL cti_op_stricteq(void** args);
        static JSValue* SFX_CALL cti_op_nstricteq(void** args);
        static JSValue* SFX_CALL cti_op_to_jsnumber(void** args);
        static JSValue* SFX_CALL cti_op_in(void** args);
        static JSValue* SFX_CALL cti_op_push_new_scope(void** args);
        static void SFX_CALL cti_op_jmp_scopes(void** args);
        static void SFX_CALL cti_op_put_by_index(void** args);
        static void* SFX_CALL cti_op_switch_imm(void** args);
        static void* SFX_CALL cti_op_switch_char(void** args);
        static void* SFX_CALL cti_op_switch_string(void** args);
        static JSValue* SFX_CALL cti_op_del_by_val(void** args);
        static void SFX_CALL cti_op_put_getter(void** args);
        static void SFX_CALL cti_op_put_setter(void** args);
        static JSValue* SFX_CALL cti_op_new_error(void** args);
        static void SFX_CALL cti_op_debug(void** args);

        static void* SFX_CALL cti_vm_throw(void** args);
        static void* SFX_CALL cti_vm_compile(void** args);
        static JSValue* SFX_CALL cti_op_push_activation(void** args);
        
#endif // ENABLE(CTI)

        // Default number of ticks before a timeout check should be done.
        static const int initialTickCountThreshold = 1024;

        bool isJSArray(JSValue* v) { return !JSImmediate::isImmediate(v) && v->asCell()->vptr() == m_jsArrayVptr; }
        bool isJSString(JSValue* v) { return !JSImmediate::isImmediate(v) && v->asCell()->vptr() == m_jsStringVptr; }

    private:
        enum ExecutionFlag { Normal, InitializeAndReturn };

        NEVER_INLINE JSValue* callEval(CallFrame*, JSObject* thisObject, ScopeChainNode*, RegisterFile*, int argv, int argc, JSValue*& exceptionValue);
        JSValue* execute(EvalNode*, CallFrame*, JSObject* thisObject, int registerOffset, ScopeChainNode*, JSValue** exception);

        NEVER_INLINE void debug(CallFrame*, DebugHookID, int firstLine, int lastLine);

        NEVER_INLINE bool resolve(CallFrame*, Instruction*, JSValue*& exceptionValue);
        NEVER_INLINE bool resolveSkip(CallFrame*, Instruction*, JSValue*& exceptionValue);
        NEVER_INLINE bool resolveGlobal(CallFrame*, Instruction*, JSValue*& exceptionValue);
        NEVER_INLINE void resolveBase(CallFrame*, Instruction* vPC);
        NEVER_INLINE bool resolveBaseAndProperty(CallFrame*, Instruction*, JSValue*& exceptionValue);
        NEVER_INLINE ScopeChainNode* createExceptionScope(CallFrame*, const Instruction* vPC);

        NEVER_INLINE bool unwindCallFrame(CallFrame*&, JSValue*, const Instruction*&, CodeBlock*&);
        NEVER_INLINE Instruction* throwException(CallFrame*&, JSValue*&, const Instruction*, bool);
        NEVER_INLINE bool resolveBaseAndFunc(CallFrame*, Instruction*, JSValue*& exceptionValue);

        static ALWAYS_INLINE CallFrame* slideRegisterWindowForCall(CodeBlock*, RegisterFile*, CallFrame*, size_t registerOffset, int argc);

        static CallFrame* findFunctionCallFrame(CallFrame*, InternalFunction*);

        JSValue* privateExecute(ExecutionFlag, RegisterFile*, CallFrame*, JSValue** exception);

        void dumpCallFrame(const RegisterFile*, CallFrame*);
        void dumpRegisters(const RegisterFile*, CallFrame*);

        JSValue* checkTimeout(JSGlobalObject*);
        void resetTimeoutCheck();

        void tryCacheGetByID(CallFrame*, CodeBlock*, Instruction*, JSValue* baseValue, const Identifier& propertyName, const PropertySlot&);
        void uncacheGetByID(CodeBlock*, Instruction* vPC);
        void tryCachePutByID(CallFrame*, CodeBlock*, Instruction*, JSValue* baseValue, const PutPropertySlot&);
        void uncachePutByID(CodeBlock*, Instruction* vPC);

#if ENABLE(CTI)
        void tryCTICacheGetByID(CallFrame*, CodeBlock*, void* returnAddress, JSValue* baseValue, const Identifier& propertyName, const PropertySlot&);
        void tryCTICachePutByID(CallFrame*, CodeBlock*, void* returnAddress, JSValue* baseValue, const PutPropertySlot&);

        void* getCTIArrayLengthTrampoline(CallFrame*, CodeBlock*);
        void* getCTIStringLengthTrampoline(CallFrame*, CodeBlock*);

        void* m_ctiArrayLengthTrampoline;
        void* m_ctiStringLengthTrampoline;

        OwnPtr<JITCodeBuffer> m_jitCodeBuffer;
        JITCodeBuffer* jitCodeBuffer() const { return m_jitCodeBuffer.get(); }
#endif

        int m_reentryDepth;
        unsigned m_timeoutTime;
        unsigned m_timeAtLastCheckTimeout;
        unsigned m_timeExecuting;
        unsigned m_timeoutCheckCount;
        unsigned m_ticksUntilNextTimeoutCheck;

        RegisterFile m_registerFile;
        
        void* m_jsArrayVptr;
        void* m_jsStringVptr;
        void* m_jsFunctionVptr;

#if HAVE(COMPUTED_GOTO)
        Opcode m_opcodeTable[numOpcodeIDs]; // Maps OpcodeID => Opcode for compiling
        HashMap<Opcode, OpcodeID> m_opcodeIDTable; // Maps Opcode => OpcodeID for decompiling
#endif
    };

} // namespace JSC

#endif // Machine_h
