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

#ifndef JITStubs_h
#define JITStubs_h

#include "Register.h"
#include <wtf/Platform.h>

#if ENABLE(JIT)

namespace JSC {

    class ExecutablePool;
    class JSGlobalData;
    class JSObject;
    class JSPropertyNameIterator;
    class JSValueEncodedAsPointer;
    class CodeBlock;
    class JSValue;
    class Identifier;
    class PropertySlot;
    class PutPropertySlot;

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

    class JITStubs {
    public:
        JITStubs(JSGlobalData*);

        static JSObject* JIT_STUB cti_op_construct_JSConstruct(STUB_ARGS);
        static JSObject* JIT_STUB cti_op_convert_this(STUB_ARGS);
        static JSObject* JIT_STUB cti_op_new_array(STUB_ARGS);
        static JSObject* JIT_STUB cti_op_new_error(STUB_ARGS);
        static JSObject* JIT_STUB cti_op_new_func(STUB_ARGS);
        static JSObject* JIT_STUB cti_op_new_func_exp(STUB_ARGS);
        static JSObject* JIT_STUB cti_op_new_object(STUB_ARGS);
        static JSObject* JIT_STUB cti_op_new_regexp(STUB_ARGS);
        static JSObject* JIT_STUB cti_op_push_activation(STUB_ARGS);
        static JSObject* JIT_STUB cti_op_push_new_scope(STUB_ARGS);
        static JSObject* JIT_STUB cti_op_push_scope(STUB_ARGS);
        static JSPropertyNameIterator* JIT_STUB cti_op_get_pnames(STUB_ARGS);
        static EncodedJSValue JIT_STUB cti_op_add(STUB_ARGS);
        static EncodedJSValue JIT_STUB cti_op_bitand(STUB_ARGS);
        static EncodedJSValue JIT_STUB cti_op_bitnot(STUB_ARGS);
        static EncodedJSValue JIT_STUB cti_op_bitor(STUB_ARGS);
        static EncodedJSValue JIT_STUB cti_op_bitxor(STUB_ARGS);
        static EncodedJSValue JIT_STUB cti_op_call_NotJSFunction(STUB_ARGS);
        static EncodedJSValue JIT_STUB cti_op_call_eval(STUB_ARGS);
        static EncodedJSValue JIT_STUB cti_op_construct_NotJSConstruct(STUB_ARGS);
        static EncodedJSValue JIT_STUB cti_op_del_by_id(STUB_ARGS);
        static EncodedJSValue JIT_STUB cti_op_del_by_val(STUB_ARGS);
        static EncodedJSValue JIT_STUB cti_op_div(STUB_ARGS);
        static EncodedJSValue JIT_STUB cti_op_eq(STUB_ARGS);
        static EncodedJSValue JIT_STUB cti_op_get_by_id(STUB_ARGS);
        static EncodedJSValue JIT_STUB cti_op_get_by_id_array_fail(STUB_ARGS);
        static EncodedJSValue JIT_STUB cti_op_get_by_id_generic(STUB_ARGS);
        static EncodedJSValue JIT_STUB cti_op_get_by_id_proto_fail(STUB_ARGS);
        static EncodedJSValue JIT_STUB cti_op_get_by_id_proto_list(STUB_ARGS);
        static EncodedJSValue JIT_STUB cti_op_get_by_id_proto_list_full(STUB_ARGS);
        static EncodedJSValue JIT_STUB cti_op_get_by_id_second(STUB_ARGS);
        static EncodedJSValue JIT_STUB cti_op_get_by_id_self_fail(STUB_ARGS);
        static EncodedJSValue JIT_STUB cti_op_get_by_id_string_fail(STUB_ARGS);
        static EncodedJSValue JIT_STUB cti_op_get_by_val(STUB_ARGS);
        static EncodedJSValue JIT_STUB cti_op_get_by_val_byte_array(STUB_ARGS);
        static EncodedJSValue JIT_STUB cti_op_get_by_val_string(STUB_ARGS);
        static EncodedJSValue JIT_STUB cti_op_in(STUB_ARGS);
        static EncodedJSValue JIT_STUB cti_op_instanceof(STUB_ARGS);
        static EncodedJSValue JIT_STUB cti_op_is_boolean(STUB_ARGS);
        static EncodedJSValue JIT_STUB cti_op_is_function(STUB_ARGS);
        static EncodedJSValue JIT_STUB cti_op_is_number(STUB_ARGS);
        static EncodedJSValue JIT_STUB cti_op_is_object(STUB_ARGS);
        static EncodedJSValue JIT_STUB cti_op_is_string(STUB_ARGS);
        static EncodedJSValue JIT_STUB cti_op_is_undefined(STUB_ARGS);
        static EncodedJSValue JIT_STUB cti_op_less(STUB_ARGS);
        static EncodedJSValue JIT_STUB cti_op_lesseq(STUB_ARGS);
        static EncodedJSValue JIT_STUB cti_op_lshift(STUB_ARGS);
        static EncodedJSValue JIT_STUB cti_op_mod(STUB_ARGS);
        static EncodedJSValue JIT_STUB cti_op_mul(STUB_ARGS);
        static EncodedJSValue JIT_STUB cti_op_negate(STUB_ARGS);
        static EncodedJSValue JIT_STUB cti_op_neq(STUB_ARGS);
        static EncodedJSValue JIT_STUB cti_op_next_pname(STUB_ARGS);
        static EncodedJSValue JIT_STUB cti_op_not(STUB_ARGS);
        static EncodedJSValue JIT_STUB cti_op_nstricteq(STUB_ARGS);
        static EncodedJSValue JIT_STUB cti_op_pre_dec(STUB_ARGS);
        static EncodedJSValue JIT_STUB cti_op_pre_inc(STUB_ARGS);
        static EncodedJSValue JIT_STUB cti_op_resolve(STUB_ARGS);
        static EncodedJSValue JIT_STUB cti_op_resolve_base(STUB_ARGS);
        static EncodedJSValue JIT_STUB cti_op_resolve_global(STUB_ARGS);
        static EncodedJSValue JIT_STUB cti_op_resolve_skip(STUB_ARGS);
        static EncodedJSValue JIT_STUB cti_op_rshift(STUB_ARGS);
        static EncodedJSValue JIT_STUB cti_op_stricteq(STUB_ARGS);
        static EncodedJSValue JIT_STUB cti_op_sub(STUB_ARGS);
        static EncodedJSValue JIT_STUB cti_op_throw(STUB_ARGS);
        static EncodedJSValue JIT_STUB cti_op_to_jsnumber(STUB_ARGS);
        static EncodedJSValue JIT_STUB cti_op_typeof(STUB_ARGS);
        static EncodedJSValue JIT_STUB cti_op_urshift(STUB_ARGS);
        static EncodedJSValue JIT_STUB cti_vm_throw(STUB_ARGS);
        static VoidPtrPair JIT_STUB cti_op_call_arityCheck(STUB_ARGS);
        static VoidPtrPair JIT_STUB cti_op_post_dec(STUB_ARGS);
        static VoidPtrPair JIT_STUB cti_op_post_inc(STUB_ARGS);
        static VoidPtrPair JIT_STUB cti_op_resolve_func(STUB_ARGS);
        static VoidPtrPair JIT_STUB cti_op_resolve_with_base(STUB_ARGS);
        static int JIT_STUB cti_op_jless(STUB_ARGS);
        static int JIT_STUB cti_op_jtrue(STUB_ARGS);
        static int JIT_STUB cti_op_loop_if_less(STUB_ARGS);
        static int JIT_STUB cti_op_loop_if_lesseq(STUB_ARGS);
        static int JIT_STUB cti_op_loop_if_true(STUB_ARGS);
        static int JIT_STUB cti_op_load_varargs(STUB_ARGS);
        static int JIT_STUB cti_timeout_check(STUB_ARGS);
        static void JIT_STUB cti_op_create_arguments(STUB_ARGS);
        static void JIT_STUB cti_op_create_arguments_no_params(STUB_ARGS);
        static void JIT_STUB cti_op_debug(STUB_ARGS);
        static void JIT_STUB cti_op_end(STUB_ARGS);
        static void JIT_STUB cti_op_jmp_scopes(STUB_ARGS);
        static void JIT_STUB cti_op_pop_scope(STUB_ARGS);
        static void JIT_STUB cti_op_profile_did_call(STUB_ARGS);
        static void JIT_STUB cti_op_profile_will_call(STUB_ARGS);
        static void JIT_STUB cti_op_put_by_id(STUB_ARGS);
        static void JIT_STUB cti_op_put_by_id_fail(STUB_ARGS);
        static void JIT_STUB cti_op_put_by_id_generic(STUB_ARGS);
        static void JIT_STUB cti_op_put_by_id_second(STUB_ARGS);
        static void JIT_STUB cti_op_put_by_index(STUB_ARGS);
        static void JIT_STUB cti_op_put_by_val(STUB_ARGS);
        static void JIT_STUB cti_op_put_by_val_array(STUB_ARGS);
        static void JIT_STUB cti_op_put_by_val_byte_array(STUB_ARGS);
        static void JIT_STUB cti_op_put_getter(STUB_ARGS);
        static void JIT_STUB cti_op_put_setter(STUB_ARGS);
        static void JIT_STUB cti_op_ret_scopeChain(STUB_ARGS);
        static void JIT_STUB cti_op_tear_off_activation(STUB_ARGS);
        static void JIT_STUB cti_op_tear_off_arguments(STUB_ARGS);
        static void JIT_STUB cti_register_file_check(STUB_ARGS);
        static void* JIT_STUB cti_op_call_JSFunction(STUB_ARGS);
        static void* JIT_STUB cti_op_switch_char(STUB_ARGS);
        static void* JIT_STUB cti_op_switch_imm(STUB_ARGS);
        static void* JIT_STUB cti_op_switch_string(STUB_ARGS);
        static void* JIT_STUB cti_vm_dontLazyLinkCall(STUB_ARGS);
        static void* JIT_STUB cti_vm_lazyLinkCall(STUB_ARGS);

        static void tryCacheGetByID(CallFrame*, CodeBlock*, void* returnAddress, JSValue baseValue, const Identifier& propertyName, const PropertySlot&);
        static void tryCachePutByID(CallFrame*, CodeBlock*, void* returnAddress, JSValue baseValue, const PutPropertySlot&);
        
        void* ctiArrayLengthTrampoline() { return m_ctiArrayLengthTrampoline; }
        void* ctiStringLengthTrampoline() { return m_ctiStringLengthTrampoline; }
        void* ctiVirtualCallPreLink() { return m_ctiVirtualCallPreLink; }
        void* ctiVirtualCallLink() { return m_ctiVirtualCallLink; }
        void* ctiVirtualCall() { return m_ctiVirtualCall; }

    private:
        RefPtr<ExecutablePool> m_executablePool;

        void* m_ctiArrayLengthTrampoline;
        void* m_ctiStringLengthTrampoline;
        void* m_ctiVirtualCallPreLink;
        void* m_ctiVirtualCallLink;
        void* m_ctiVirtualCall;
    };

} // namespace JSC

#endif // ENABLE(JIT)

#endif // JITStubs_h
