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

#if USE(CTI_ARGUMENT)
#define CTI_ARGS void** args
#define ARGS (args)
#else
#define CTI_ARGS void* args, ...
#define ARGS (&args)
#endif

#if USE(FAST_CALL_CTI_ARGUMENT)

#if COMPILER(MSVC)
#define SFX_CALL __fastcall
#elif COMPILER(GCC)
#define SFX_CALL  __attribute__ ((fastcall))
#else
#error Need to support fastcall calling convention in this compiler
#endif

#else

#if COMPILER(MSVC)
#define SFX_CALL __cdecl
#else
#define SFX_CALL
#endif

#endif

    typedef uint64_t VoidPtrPair;

    typedef union
    {
        struct { void* first; void* second; } s;
        VoidPtrPair i;
    } VoidPtrPairValue;
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

        void setSampler(SamplingTool* sampler) { m_sampler = sampler; }
        SamplingTool* sampler() { return m_sampler; }

#if ENABLE(CTI)

        static void SFX_CALL cti_timeout_check(CTI_ARGS);
        static void SFX_CALL cti_register_file_check(CTI_ARGS);

        static JSObject* SFX_CALL cti_op_convert_this(CTI_ARGS);
        static void SFX_CALL cti_op_end(CTI_ARGS);
        static JSValue* SFX_CALL cti_op_add(CTI_ARGS);
        static JSValue* SFX_CALL cti_op_pre_inc(CTI_ARGS);
        static int SFX_CALL cti_op_loop_if_less(CTI_ARGS);
        static int SFX_CALL cti_op_loop_if_lesseq(CTI_ARGS);
        static JSObject* SFX_CALL cti_op_new_object(CTI_ARGS);
        static void SFX_CALL cti_op_put_by_id(CTI_ARGS);
        static void SFX_CALL cti_op_put_by_id_second(CTI_ARGS);
        static void SFX_CALL cti_op_put_by_id_generic(CTI_ARGS);
        static void SFX_CALL cti_op_put_by_id_fail(CTI_ARGS);
        static JSValue* SFX_CALL cti_op_get_by_id(CTI_ARGS);
        static JSValue* SFX_CALL cti_op_get_by_id_second(CTI_ARGS);
        static JSValue* SFX_CALL cti_op_get_by_id_generic(CTI_ARGS);
        static JSValue* SFX_CALL cti_op_get_by_id_fail(CTI_ARGS);
        static JSValue* SFX_CALL cti_op_del_by_id(CTI_ARGS);
        static JSValue* SFX_CALL cti_op_instanceof(CTI_ARGS);
        static JSValue* SFX_CALL cti_op_mul(CTI_ARGS);
        static JSObject* SFX_CALL cti_op_new_func(CTI_ARGS);
        static void* SFX_CALL cti_op_call_JSFunction(CTI_ARGS);
        static VoidPtrPair SFX_CALL cti_op_call_arityCheck(CTI_ARGS);
        static JSValue* SFX_CALL cti_op_call_NotJSFunction(CTI_ARGS);
        static void SFX_CALL cti_op_create_arguments(CTI_ARGS);
        static void SFX_CALL cti_op_create_arguments_no_params(CTI_ARGS);
        static void SFX_CALL cti_op_tear_off_activation(CTI_ARGS);
        static void SFX_CALL cti_op_tear_off_arguments(CTI_ARGS);
        static void SFX_CALL cti_op_profile_will_call(CTI_ARGS);
        static void SFX_CALL cti_op_profile_did_call(CTI_ARGS);
        static void SFX_CALL cti_op_ret_scopeChain(CTI_ARGS);
        static JSObject* SFX_CALL cti_op_new_array(CTI_ARGS);
        static JSValue* SFX_CALL cti_op_resolve(CTI_ARGS);
        static JSValue* SFX_CALL cti_op_resolve_global(CTI_ARGS);
        static JSObject* SFX_CALL cti_op_construct_JSConstruct(CTI_ARGS);
        static JSValue* SFX_CALL cti_op_construct_NotJSConstruct(CTI_ARGS);
        static JSValue* SFX_CALL cti_op_get_by_val(CTI_ARGS);
        static VoidPtrPair SFX_CALL cti_op_resolve_func(CTI_ARGS);
        static JSValue* SFX_CALL cti_op_sub(CTI_ARGS);
        static void SFX_CALL cti_op_put_by_val(CTI_ARGS);
        static void SFX_CALL cti_op_put_by_val_array(CTI_ARGS);
        static JSValue* SFX_CALL cti_op_lesseq(CTI_ARGS);
        static int SFX_CALL cti_op_loop_if_true(CTI_ARGS);
        static JSValue* SFX_CALL cti_op_resolve_base(CTI_ARGS);
        static JSValue* SFX_CALL cti_op_negate(CTI_ARGS);
        static JSValue* SFX_CALL cti_op_resolve_skip(CTI_ARGS);
        static JSValue* SFX_CALL cti_op_div(CTI_ARGS);
        static JSValue* SFX_CALL cti_op_pre_dec(CTI_ARGS);
        static int SFX_CALL cti_op_jless(CTI_ARGS);
        static JSValue* SFX_CALL cti_op_not(CTI_ARGS);
        static int SFX_CALL cti_op_jtrue(CTI_ARGS);
        static VoidPtrPair SFX_CALL cti_op_post_inc(CTI_ARGS);
        static JSValue* SFX_CALL cti_op_eq(CTI_ARGS);
        static JSValue* SFX_CALL cti_op_lshift(CTI_ARGS);
        static JSValue* SFX_CALL cti_op_bitand(CTI_ARGS);
        static JSValue* SFX_CALL cti_op_rshift(CTI_ARGS);
        static JSValue* SFX_CALL cti_op_bitnot(CTI_ARGS);
        static VoidPtrPair SFX_CALL cti_op_resolve_with_base(CTI_ARGS);
        static JSObject* SFX_CALL cti_op_new_func_exp(CTI_ARGS);
        static JSValue* SFX_CALL cti_op_mod(CTI_ARGS);
        static JSValue* SFX_CALL cti_op_less(CTI_ARGS);
        static JSValue* SFX_CALL cti_op_neq(CTI_ARGS);
        static VoidPtrPair SFX_CALL cti_op_post_dec(CTI_ARGS);
        static JSValue* SFX_CALL cti_op_urshift(CTI_ARGS);
        static JSValue* SFX_CALL cti_op_bitxor(CTI_ARGS);
        static JSObject* SFX_CALL cti_op_new_regexp(CTI_ARGS);
        static JSValue* SFX_CALL cti_op_bitor(CTI_ARGS);
        static JSValue* SFX_CALL cti_op_call_eval(CTI_ARGS);
        static JSValue* SFX_CALL cti_op_throw(CTI_ARGS);
        static JSPropertyNameIterator* SFX_CALL cti_op_get_pnames(CTI_ARGS);
        static JSValue* SFX_CALL cti_op_next_pname(CTI_ARGS);
        static void SFX_CALL cti_op_push_scope(CTI_ARGS);
        static void SFX_CALL cti_op_pop_scope(CTI_ARGS);
        static JSValue* SFX_CALL cti_op_typeof(CTI_ARGS);
        static JSValue* SFX_CALL cti_op_is_undefined(CTI_ARGS);
        static JSValue* SFX_CALL cti_op_is_boolean(CTI_ARGS);
        static JSValue* SFX_CALL cti_op_is_number(CTI_ARGS);
        static JSValue* SFX_CALL cti_op_is_string(CTI_ARGS);
        static JSValue* SFX_CALL cti_op_is_object(CTI_ARGS);
        static JSValue* SFX_CALL cti_op_is_function(CTI_ARGS);
        static JSValue* SFX_CALL cti_op_stricteq(CTI_ARGS);
        static JSValue* SFX_CALL cti_op_nstricteq(CTI_ARGS);
        static JSValue* SFX_CALL cti_op_to_jsnumber(CTI_ARGS);
        static JSValue* SFX_CALL cti_op_in(CTI_ARGS);
        static JSObject* SFX_CALL cti_op_push_new_scope(CTI_ARGS);
        static void SFX_CALL cti_op_jmp_scopes(CTI_ARGS);
        static void SFX_CALL cti_op_put_by_index(CTI_ARGS);
        static void* SFX_CALL cti_op_switch_imm(CTI_ARGS);
        static void* SFX_CALL cti_op_switch_char(CTI_ARGS);
        static void* SFX_CALL cti_op_switch_string(CTI_ARGS);
        static JSValue* SFX_CALL cti_op_del_by_val(CTI_ARGS);
        static void SFX_CALL cti_op_put_getter(CTI_ARGS);
        static void SFX_CALL cti_op_put_setter(CTI_ARGS);
        static JSObject* SFX_CALL cti_op_new_error(CTI_ARGS);
        static void SFX_CALL cti_op_debug(CTI_ARGS);

        static JSValue* SFX_CALL cti_allocate_number(CTI_ARGS);

        static JSValue* SFX_CALL cti_vm_throw(CTI_ARGS);
        static void* SFX_CALL cti_vm_lazyLinkCall(CTI_ARGS);
        static JSObject* SFX_CALL cti_op_push_activation(CTI_ARGS);
        
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
        
        bool isCallOpcode(Opcode opcode) { return opcode == getOpcode(op_call) || opcode == getOpcode(op_construct) || opcode == getOpcode(op_call_eval); }

#if ENABLE(CTI)
        static void throwStackOverflowPreviousFrame(CallFrame*, JSGlobalData*, void*& returnAddress);

        void tryCTICacheGetByID(CallFrame*, CodeBlock*, void* returnAddress, JSValue* baseValue, const Identifier& propertyName, const PropertySlot&);
        void tryCTICachePutByID(CallFrame*, CodeBlock*, void* returnAddress, JSValue* baseValue, const PutPropertySlot&);

        void* getCTIArrayLengthTrampoline(CallFrame*, CodeBlock*);
        void* getCTIStringLengthTrampoline(CallFrame*, CodeBlock*);

        JITCodeBuffer* jitCodeBuffer() const { return m_jitCodeBuffer.get(); }
#endif

        SamplingTool* m_sampler;

#if ENABLE(CTI)
        void* m_ctiArrayLengthTrampoline;
        void* m_ctiStringLengthTrampoline;

        OwnPtr<JITCodeBuffer> m_jitCodeBuffer;
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
