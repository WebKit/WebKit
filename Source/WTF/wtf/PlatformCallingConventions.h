/*
 * Copyright (C) 2006-2021 Apple Inc. All rights reserved.
 * Copyright (C) 2007-2009 Torch Mobile, Inc.
 * Copyright (C) 2010, 2011 Research In Motion Limited. All rights reserved.
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

#pragma once

#ifndef WTF_PLATFORM_GUARD_AGAINST_INDIRECT_INCLUSION
#error "Please #include <wtf/Platform.h> instead of this file directly."
#endif

/* Macros for specifing specific calling conventions. */


#if CPU(X86) && COMPILER(MSVC)
#define JSC_HOST_CALL_ATTRIBUTES __fastcall
#elif CPU(X86) && COMPILER(GCC_COMPATIBLE)
#define JSC_HOST_CALL_ATTRIBUTES __attribute__ ((fastcall))
#else
#define JSC_HOST_CALL_ATTRIBUTES
#endif

#define JSC_ANNOTATE_HOST_FUNCTION(functionId, function)

#define JSC_DEFINE_HOST_FUNCTION_WITH_ATTRIBUTES(functionName, attributes, parameters) \
    JSC_ANNOTATE_HOST_FUNCTION(_JITTarget_##functionName, static_cast<JSC::EncodedJSValue(*)parameters>(functionName)); \
    attributes JSC::EncodedJSValue JSC_HOST_CALL_ATTRIBUTES functionName parameters
#define JSC_DEFINE_HOST_FUNCTION(functionName, parameters) \
    JSC_DEFINE_HOST_FUNCTION_WITH_ATTRIBUTES(functionName, , parameters)
#define JSC_DECLARE_HOST_FUNCTION(functionName) \
    JSC::EncodedJSValue JSC_HOST_CALL_ATTRIBUTES functionName(JSC::JSGlobalObject*, JSC::CallFrame*)

#if CPU(X86) && OS(WINDOWS)
#define CALLING_CONVENTION_IS_STDCALL 1
#ifndef CDECL
#if COMPILER(MSVC)
#define CDECL __cdecl
#else
#define CDECL __attribute__ ((__cdecl))
#endif
#endif
#else
#define CALLING_CONVENTION_IS_STDCALL 0
#endif

#if CPU(X86)
#define WTF_COMPILER_SUPPORTS_FASTCALL_CALLING_CONVENTION 1
#ifndef FASTCALL
#if COMPILER(MSVC)
#define FASTCALL __fastcall
#else
#define FASTCALL  __attribute__ ((fastcall))
#endif
#endif
#else
#define WTF_COMPILER_SUPPORTS_FASTCALL_CALLING_CONVENTION 0
#endif

#if ENABLE(JIT) && CALLING_CONVENTION_IS_STDCALL
#define JIT_OPERATION_ATTRIBUTES CDECL
#else
#define JIT_OPERATION_ATTRIBUTES
#endif

#if ENABLE(JIT_OPERATION_VALIDATION)
#define JSC_ANNOTATE_JIT_OPERATION_INTERNAL(function) \
    constexpr JSC::JITOperationAnnotation _JITTargetID_##function __attribute__((used, section("__DATA_CONST,__jsc_ops"))) = { (void*)function, (void*)function##Validate };

#define JSC_ANNOTATE_JIT_OPERATION(function) \
    JSC_DECLARE_AND_DEFINE_JIT_OPERATION_VALIDATION(function); \
    JSC_ANNOTATE_JIT_OPERATION_INTERNAL(function)

#define JSC_ANNOTATE_JIT_OPERATION_PROBE(function) \
    JSC_DECLARE_AND_DEFINE_JIT_OPERATION_PROBE_VALIDATION(function); \
    JSC_ANNOTATE_JIT_OPERATION_INTERNAL(function)

#define JSC_ANNOTATE_JIT_OPERATION_RETURN(function) \
    JSC_DECLARE_AND_DEFINE_JIT_OPERATION_RETURN_VALIDATION(function); \
    JSC_ANNOTATE_JIT_OPERATION_INTERNAL(function)

#else
#define JSC_ANNOTATE_JIT_OPERATION(function)
#define JSC_ANNOTATE_JIT_OPERATION_PROBE(function)
#define JSC_ANNOTATE_JIT_OPERATION_RETURN(function)
#endif


#define JSC_DEFINE_JIT_OPERATION_WITHOUT_VARIABLE(functionName, returnType, parameters) \
    returnType JIT_OPERATION_ATTRIBUTES functionName parameters

#define JSC_DEFINE_JIT_OPERATION_WITH_ATTRIBUTES(functionName, attributes, returnType, parameters) \
    JSC_ANNOTATE_JIT_OPERATION(functionName); \
    attributes returnType JIT_OPERATION_ATTRIBUTES functionName parameters

#define JSC_DEFINE_JIT_OPERATION(functionName, returnType, parameters) \
    JSC_DEFINE_JIT_OPERATION_WITH_ATTRIBUTES(functionName, , returnType, parameters)

#define JSC_DECLARE_JIT_OPERATION_WITH_ATTRIBUTES(functionName, attributes, returnType, parameters) \
    extern "C" attributes returnType JIT_OPERATION_ATTRIBUTES functionName parameters REFERENCED_FROM_ASM WTF_INTERNAL; \
    JSC_DECLARE_JIT_OPERATION_VALIDATION(functionName) \

#define JSC_DECLARE_JIT_OPERATION(functionName, returnType, parameters) \
    JSC_DECLARE_JIT_OPERATION_WITH_ATTRIBUTES(functionName, , returnType, parameters)

#define JSC_DECLARE_JIT_OPERATION_WITHOUT_WTF_INTERNAL(functionName, returnType, parameters) \
    returnType JIT_OPERATION_ATTRIBUTES functionName parameters REFERENCED_FROM_ASM

#define JSC_DECLARE_CUSTOM_GETTER(functionName) JSC_DECLARE_JIT_OPERATION_WITHOUT_WTF_INTERNAL(functionName, JSC::EncodedJSValue, (JSC::JSGlobalObject*, JSC::EncodedJSValue, JSC::PropertyName))
#define JSC_DECLARE_CUSTOM_SETTER(functionName) JSC_DECLARE_JIT_OPERATION_WITHOUT_WTF_INTERNAL(functionName, bool, (JSC::JSGlobalObject*, JSC::EncodedJSValue, JSC::EncodedJSValue, JSC::PropertyName))
#define JSC_DEFINE_CUSTOM_GETTER(functionName, parameters) JSC_DEFINE_JIT_OPERATION_WITHOUT_VARIABLE(functionName, JSC::EncodedJSValue, parameters)
#define JSC_DEFINE_CUSTOM_SETTER(functionName, parameters) JSC_DEFINE_JIT_OPERATION_WITHOUT_VARIABLE(functionName, bool, parameters)
