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

#ifndef Interpreter_h
#define Interpreter_h

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
    class AssemblerBuffer;
    class JSFunction;
    class JSGlobalObject;
    class ProgramNode;
    class Register;
    class ScopeChainNode;
    class SamplingTool;
    struct HandlerInfo;

#if ENABLE(JIT)

#if USE(JIT_STUB_ARGUMENT_VA_LIST)
    #define STUB_ARGS void* args, ...
    #define ARGS (reinterpret_cast<void**>(vl_args) - 1)
#else // JIT_STUB_ARGUMENT_REGISTER or JIT_STUB_ARGUMENT_STACK
    #define STUB_ARGS void** args
    #define ARGS (args)
#endif

#if USE(JIT_STUB_ARGUMENT_REGISTER)
    #if PLATFORM(X86_64)
    #define JIT_STUB
    #elif COMPILER(MSVC)
    #define JIT_STUB __fastcall
    #elif COMPILER(GCC)
    #define JIT_STUB  __attribute__ ((fastcall))
    #else
    #error Need to support register calling convention in this compiler
    #endif
#else // JIT_STUB_ARGUMENT_VA_LIST or JIT_STUB_ARGUMENT_STACK
    #if COMPILER(MSVC)
    #define JIT_STUB __cdecl
    #else
    #define JIT_STUB
    #endif
#endif

// The Mac compilers are fine with this, 
#if PLATFORM(MAC)
    struct VoidPtrPair {
        void* first;
        void* second;
    };
#define RETURN_PAIR(a,b) VoidPtrPair pair = { a, b }; return pair
#else
    typedef uint64_t VoidPtrPair;
    union VoidPtrPairValue {
        struct { void* first; void* second; } s;
        VoidPtrPair i;
    };
#define RETURN_PAIR(a,b) VoidPtrPairValue pair = {{ a, b }}; return pair.i
#endif

#endif // ENABLE(JIT)

    enum DebugHookID {
        WillExecuteProgram,
        DidExecuteProgram,
        DidEnterCallFrame,
        DidReachBreakpoint,
        WillLeaveCallFrame,
        WillExecuteStatement
    };

    enum { MaxReentryDepth = 128 };

    class Interpreter {
        friend class JIT;
    public:
        Interpreter();
        ~Interpreter();

        void initialize(JSGlobalData*);
        
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

        bool isOpcode(Opcode);
        
        JSValuePtr execute(ProgramNode*, CallFrame*, ScopeChainNode*, JSObject* thisObj, JSValuePtr* exception);
        JSValuePtr execute(FunctionBodyNode*, CallFrame*, JSFunction*, JSObject* thisObj, const ArgList& args, ScopeChainNode*, JSValuePtr* exception);
        JSValuePtr execute(EvalNode* evalNode, CallFrame* exec, JSObject* thisObj, ScopeChainNode* scopeChain, JSValuePtr* exception);

        JSValuePtr retrieveArguments(CallFrame*, JSFunction*) const;
        JSValuePtr retrieveCaller(CallFrame*, InternalFunction*) const;
        void retrieveLastCaller(CallFrame*, int& lineNumber, intptr_t& sourceID, UString& sourceURL, JSValuePtr& function) const;
        
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

#if ENABLE(JIT)

        static int JIT_STUB cti_timeout_check(STUB_ARGS);
        static void JIT_STUB cti_register_file_check(STUB_ARGS);

        static JSObject* JIT_STUB cti_op_convert_this(STUB_ARGS);
        static void JIT_STUB cti_op_end(STUB_ARGS);
        static JSValueEncodedAsPointer* JIT_STUB cti_op_add(STUB_ARGS);
        static JSValueEncodedAsPointer* JIT_STUB cti_op_pre_inc(STUB_ARGS);
        static int JIT_STUB cti_op_loop_if_less(STUB_ARGS);
        static int JIT_STUB cti_op_loop_if_lesseq(STUB_ARGS);
        static JSObject* JIT_STUB cti_op_new_object(STUB_ARGS);
        static void JIT_STUB cti_op_put_by_id(STUB_ARGS);
        static void JIT_STUB cti_op_put_by_id_second(STUB_ARGS);
        static void JIT_STUB cti_op_put_by_id_generic(STUB_ARGS);
        static void JIT_STUB cti_op_put_by_id_fail(STUB_ARGS);
        static JSValueEncodedAsPointer* JIT_STUB cti_op_get_by_id(STUB_ARGS);
        static JSValueEncodedAsPointer* JIT_STUB cti_op_get_by_id_second(STUB_ARGS);
        static JSValueEncodedAsPointer* JIT_STUB cti_op_get_by_id_generic(STUB_ARGS);
        static JSValueEncodedAsPointer* JIT_STUB cti_op_get_by_id_self_fail(STUB_ARGS);
        static JSValueEncodedAsPointer* JIT_STUB cti_op_get_by_id_proto_list(STUB_ARGS);
        static JSValueEncodedAsPointer* JIT_STUB cti_op_get_by_id_proto_list_full(STUB_ARGS);
        static JSValueEncodedAsPointer* JIT_STUB cti_op_get_by_id_proto_fail(STUB_ARGS);
        static JSValueEncodedAsPointer* JIT_STUB cti_op_get_by_id_array_fail(STUB_ARGS);
        static JSValueEncodedAsPointer* JIT_STUB cti_op_get_by_id_string_fail(STUB_ARGS);
        static JSValueEncodedAsPointer* JIT_STUB cti_op_del_by_id(STUB_ARGS);
        static JSValueEncodedAsPointer* JIT_STUB cti_op_instanceof(STUB_ARGS);
        static JSValueEncodedAsPointer* JIT_STUB cti_op_mul(STUB_ARGS);
        static JSObject* JIT_STUB cti_op_new_func(STUB_ARGS);
        static void* JIT_STUB cti_op_call_JSFunction(STUB_ARGS);
        static VoidPtrPair JIT_STUB cti_op_call_arityCheck(STUB_ARGS);
        static JSValueEncodedAsPointer* JIT_STUB cti_op_call_NotJSFunction(STUB_ARGS);
        static void JIT_STUB cti_op_create_arguments(STUB_ARGS);
        static void JIT_STUB cti_op_create_arguments_no_params(STUB_ARGS);
        static void JIT_STUB cti_op_tear_off_activation(STUB_ARGS);
        static void JIT_STUB cti_op_tear_off_arguments(STUB_ARGS);
        static void JIT_STUB cti_op_profile_will_call(STUB_ARGS);
        static void JIT_STUB cti_op_profile_did_call(STUB_ARGS);
        static void JIT_STUB cti_op_ret_scopeChain(STUB_ARGS);
        static JSObject* JIT_STUB cti_op_new_array(STUB_ARGS);
        static JSValueEncodedAsPointer* JIT_STUB cti_op_resolve(STUB_ARGS);
        static JSValueEncodedAsPointer* JIT_STUB cti_op_resolve_global(STUB_ARGS);
        static JSObject* JIT_STUB cti_op_construct_JSConstruct(STUB_ARGS);
        static JSValueEncodedAsPointer* JIT_STUB cti_op_construct_NotJSConstruct(STUB_ARGS);
        static JSValueEncodedAsPointer* JIT_STUB cti_op_get_by_val(STUB_ARGS);
        static JSValueEncodedAsPointer* JIT_STUB cti_op_get_by_val_byte_array(STUB_ARGS);
        static VoidPtrPair JIT_STUB cti_op_resolve_func(STUB_ARGS);
        static JSValueEncodedAsPointer* JIT_STUB cti_op_sub(STUB_ARGS);
        static void JIT_STUB cti_op_put_by_val(STUB_ARGS);
        static void JIT_STUB cti_op_put_by_val_array(STUB_ARGS);
        static void JIT_STUB cti_op_put_by_val_byte_array(STUB_ARGS);
        static JSValueEncodedAsPointer* JIT_STUB cti_op_lesseq(STUB_ARGS);
        static int JIT_STUB cti_op_loop_if_true(STUB_ARGS);
        static JSValueEncodedAsPointer* JIT_STUB cti_op_resolve_base(STUB_ARGS);
        static JSValueEncodedAsPointer* JIT_STUB cti_op_negate(STUB_ARGS);
        static JSValueEncodedAsPointer* JIT_STUB cti_op_resolve_skip(STUB_ARGS);
        static JSValueEncodedAsPointer* JIT_STUB cti_op_div(STUB_ARGS);
        static JSValueEncodedAsPointer* JIT_STUB cti_op_pre_dec(STUB_ARGS);
        static int JIT_STUB cti_op_jless(STUB_ARGS);
        static JSValueEncodedAsPointer* JIT_STUB cti_op_not(STUB_ARGS);
        static int JIT_STUB cti_op_jtrue(STUB_ARGS);
        static VoidPtrPair JIT_STUB cti_op_post_inc(STUB_ARGS);
        static JSValueEncodedAsPointer* JIT_STUB cti_op_eq(STUB_ARGS);
        static JSValueEncodedAsPointer* JIT_STUB cti_op_lshift(STUB_ARGS);
        static JSValueEncodedAsPointer* JIT_STUB cti_op_bitand(STUB_ARGS);
        static JSValueEncodedAsPointer* JIT_STUB cti_op_rshift(STUB_ARGS);
        static JSValueEncodedAsPointer* JIT_STUB cti_op_bitnot(STUB_ARGS);
        static VoidPtrPair JIT_STUB cti_op_resolve_with_base(STUB_ARGS);
        static JSObject* JIT_STUB cti_op_new_func_exp(STUB_ARGS);
        static JSValueEncodedAsPointer* JIT_STUB cti_op_mod(STUB_ARGS);
        static JSValueEncodedAsPointer* JIT_STUB cti_op_less(STUB_ARGS);
        static JSValueEncodedAsPointer* JIT_STUB cti_op_neq(STUB_ARGS);
        static VoidPtrPair JIT_STUB cti_op_post_dec(STUB_ARGS);
        static JSValueEncodedAsPointer* JIT_STUB cti_op_urshift(STUB_ARGS);
        static JSValueEncodedAsPointer* JIT_STUB cti_op_bitxor(STUB_ARGS);
        static JSObject* JIT_STUB cti_op_new_regexp(STUB_ARGS);
        static JSValueEncodedAsPointer* JIT_STUB cti_op_bitor(STUB_ARGS);
        static JSValueEncodedAsPointer* JIT_STUB cti_op_call_eval(STUB_ARGS);
        static JSValueEncodedAsPointer* JIT_STUB cti_op_throw(STUB_ARGS);
        static JSPropertyNameIterator* JIT_STUB cti_op_get_pnames(STUB_ARGS);
        static JSValueEncodedAsPointer* JIT_STUB cti_op_next_pname(STUB_ARGS);
        static JSObject* JIT_STUB cti_op_push_scope(STUB_ARGS);
        static void JIT_STUB cti_op_pop_scope(STUB_ARGS);
        static JSValueEncodedAsPointer* JIT_STUB cti_op_typeof(STUB_ARGS);
        static JSValueEncodedAsPointer* JIT_STUB cti_op_is_undefined(STUB_ARGS);
        static JSValueEncodedAsPointer* JIT_STUB cti_op_is_boolean(STUB_ARGS);
        static JSValueEncodedAsPointer* JIT_STUB cti_op_is_number(STUB_ARGS);
        static JSValueEncodedAsPointer* JIT_STUB cti_op_is_string(STUB_ARGS);
        static JSValueEncodedAsPointer* JIT_STUB cti_op_is_object(STUB_ARGS);
        static JSValueEncodedAsPointer* JIT_STUB cti_op_is_function(STUB_ARGS);
        static JSValueEncodedAsPointer* JIT_STUB cti_op_stricteq(STUB_ARGS);
        static JSValueEncodedAsPointer* JIT_STUB cti_op_nstricteq(STUB_ARGS);
        static JSValueEncodedAsPointer* JIT_STUB cti_op_to_jsnumber(STUB_ARGS);
        static JSValueEncodedAsPointer* JIT_STUB cti_op_in(STUB_ARGS);
        static JSObject* JIT_STUB cti_op_push_new_scope(STUB_ARGS);
        static void JIT_STUB cti_op_jmp_scopes(STUB_ARGS);
        static void JIT_STUB cti_op_put_by_index(STUB_ARGS);
        static void* JIT_STUB cti_op_switch_imm(STUB_ARGS);
        static void* JIT_STUB cti_op_switch_char(STUB_ARGS);
        static void* JIT_STUB cti_op_switch_string(STUB_ARGS);
        static JSValueEncodedAsPointer* JIT_STUB cti_op_del_by_val(STUB_ARGS);
        static void JIT_STUB cti_op_put_getter(STUB_ARGS);
        static void JIT_STUB cti_op_put_setter(STUB_ARGS);
        static JSObject* JIT_STUB cti_op_new_error(STUB_ARGS);
        static void JIT_STUB cti_op_debug(STUB_ARGS);

        static JSValueEncodedAsPointer* JIT_STUB cti_vm_throw(STUB_ARGS);
        static void* JIT_STUB cti_vm_dontLazyLinkCall(STUB_ARGS);
        static void* JIT_STUB cti_vm_lazyLinkCall(STUB_ARGS);
        static JSObject* JIT_STUB cti_op_push_activation(STUB_ARGS);
        
#endif // ENABLE(JIT)

        // Default number of ticks before a timeout check should be done.
        static const int initialTickCountThreshold = 1024;

        bool isJSArray(JSValuePtr v) { return !JSImmediate::isImmediate(v) && v->asCell()->vptr() == m_jsArrayVptr; }
        bool isJSString(JSValuePtr v) { return !JSImmediate::isImmediate(v) && v->asCell()->vptr() == m_jsStringVptr; }
        bool isJSByteArray(JSValuePtr v) { return !JSImmediate::isImmediate(v) && v->asCell()->vptr() == m_jsByteArrayVptr; }

    private:
        enum ExecutionFlag { Normal, InitializeAndReturn };

        NEVER_INLINE JSValuePtr callEval(CallFrame*, RegisterFile*, Register* argv, int argc, int registerOffset, JSValuePtr& exceptionValue);
        JSValuePtr execute(EvalNode*, CallFrame*, JSObject* thisObject, int globalRegisterOffset, ScopeChainNode*, JSValuePtr* exception);

        NEVER_INLINE void debug(CallFrame*, DebugHookID, int firstLine, int lastLine);

        NEVER_INLINE bool resolve(CallFrame*, Instruction*, JSValuePtr& exceptionValue);
        NEVER_INLINE bool resolveSkip(CallFrame*, Instruction*, JSValuePtr& exceptionValue);
        NEVER_INLINE bool resolveGlobal(CallFrame*, Instruction*, JSValuePtr& exceptionValue);
        NEVER_INLINE void resolveBase(CallFrame*, Instruction* vPC);
        NEVER_INLINE bool resolveBaseAndProperty(CallFrame*, Instruction*, JSValuePtr& exceptionValue);
        NEVER_INLINE ScopeChainNode* createExceptionScope(CallFrame*, const Instruction* vPC);

        NEVER_INLINE bool unwindCallFrame(CallFrame*&, JSValuePtr, unsigned& bytecodeOffset, CodeBlock*&);
        NEVER_INLINE HandlerInfo* throwException(CallFrame*&, JSValuePtr&, unsigned bytecodeOffset, bool);
        NEVER_INLINE bool resolveBaseAndFunc(CallFrame*, Instruction*, JSValuePtr& exceptionValue);

        static ALWAYS_INLINE CallFrame* slideRegisterWindowForCall(CodeBlock*, RegisterFile*, CallFrame*, size_t registerOffset, int argc);

        static CallFrame* findFunctionCallFrame(CallFrame*, InternalFunction*);

        JSValuePtr privateExecute(ExecutionFlag, RegisterFile*, CallFrame*, JSValuePtr* exception);

        void dumpCallFrame(CallFrame*);
        void dumpRegisters(CallFrame*);

        bool checkTimeout(JSGlobalObject*);
        void resetTimeoutCheck();

        void tryCacheGetByID(CallFrame*, CodeBlock*, Instruction*, JSValuePtr baseValue, const Identifier& propertyName, const PropertySlot&);
        void uncacheGetByID(CodeBlock*, Instruction* vPC);
        void tryCachePutByID(CallFrame*, CodeBlock*, Instruction*, JSValuePtr baseValue, const PutPropertySlot&);
        void uncachePutByID(CodeBlock*, Instruction* vPC);
        
        bool isCallBytecode(Opcode opcode) { return opcode == getOpcode(op_call) || opcode == getOpcode(op_construct) || opcode == getOpcode(op_call_eval); }

#if ENABLE(JIT)
        static void throwStackOverflowPreviousFrame(CallFrame**, JSGlobalData*, void*& returnAddress);

        void tryCTICacheGetByID(CallFrame*, CodeBlock*, void* returnAddress, JSValuePtr baseValue, const Identifier& propertyName, const PropertySlot&);
        void tryCTICachePutByID(CallFrame*, CodeBlock*, void* returnAddress, JSValuePtr baseValue, const PutPropertySlot&);
#endif

        SamplingTool* m_sampler;

#if ENABLE(JIT)
        RefPtr<ExecutablePool> m_executablePool;
        void* m_ctiArrayLengthTrampoline;
        void* m_ctiStringLengthTrampoline;
        void* m_ctiVirtualCallPreLink;
        void* m_ctiVirtualCallLink;
        void* m_ctiVirtualCall;
#endif

        int m_reentryDepth;
        unsigned m_timeoutTime;
        unsigned m_timeAtLastCheckTimeout;
        unsigned m_timeExecuting;
        unsigned m_timeoutCheckCount;
        unsigned m_ticksUntilNextTimeoutCheck;

        RegisterFile m_registerFile;
        
        void* m_jsArrayVptr;
        void* m_jsByteArrayVptr;
        void* m_jsStringVptr;
        void* m_jsFunctionVptr;

#if HAVE(COMPUTED_GOTO)
        Opcode m_opcodeTable[numOpcodeIDs]; // Maps OpcodeID => Opcode for compiling
        HashMap<Opcode, OpcodeID> m_opcodeIDTable; // Maps Opcode => OpcodeID for decompiling
#endif
    };

} // namespace JSC

#endif // Interpreter_h
