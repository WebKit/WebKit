/*
 * Copyright (C) 2013 Apple Inc. All rights reserved.
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
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#include "config.h"

#include "StructuredExceptionHandlerSuppressor.h"

extern "C" int __stdcall exceptionHandlerThunk(); // Defined in makesafeseh.asm

static bool exceptionShouldTerminateProgram(int code)
{
    switch (code) {
#ifndef NDEBUG
    case EXCEPTION_DATATYPE_MISALIGNMENT:
    case EXCEPTION_FLT_DENORMAL_OPERAND:
    case EXCEPTION_FLT_DIVIDE_BY_ZERO:
    case EXCEPTION_FLT_INEXACT_RESULT:
    case EXCEPTION_FLT_INVALID_OPERATION:
    case EXCEPTION_FLT_OVERFLOW:
    case EXCEPTION_FLT_STACK_CHECK:
    case EXCEPTION_FLT_UNDERFLOW:
#endif
    case EXCEPTION_ACCESS_VIOLATION:
    case EXCEPTION_ARRAY_BOUNDS_EXCEEDED:
    case EXCEPTION_INT_DIVIDE_BY_ZERO:
    case EXCEPTION_INT_OVERFLOW:
    case EXCEPTION_PRIV_INSTRUCTION:
    case EXCEPTION_IN_PAGE_ERROR:
    case EXCEPTION_ILLEGAL_INSTRUCTION:
    case EXCEPTION_NONCONTINUABLE_EXCEPTION:
    case EXCEPTION_STACK_OVERFLOW:
    case EXCEPTION_INVALID_DISPOSITION:
    case EXCEPTION_GUARD_PAGE:
    case EXCEPTION_INVALID_HANDLE:
        return true;
    };

    return false;
}

extern "C" EXCEPTION_DISPOSITION __stdcall exceptionHandler(struct _EXCEPTION_RECORD* exceptionRecord, void* /*establisherFrame*/, struct _CONTEXT* /*contextRecord*/, void* /*dispatcherContext*/)
{
    if (exceptionShouldTerminateProgram(exceptionRecord->ExceptionCode))
        abort();

    return ExceptionContinueSearch;
}

namespace WebCore {

#pragma warning(push)
#pragma warning(disable: 4733) // Disable "not registered as safe handler" warning

StructuredExceptionHandlerSuppressor::StructuredExceptionHandlerSuppressor(ExceptionRegistration& exceptionRegistration)
{
    // Note: Windows requires that the EXCEPTION_REGISTRATION block (modeled here as our
    // ExceptionRegistration struct) be stack allocated. Therefore we instantiated it prior
    // to building this object so that Windows can still find it in stack memory when it
    // attempts to use the handler.

    // Windows puts an __try/__except block around some calls, such as hooks.
    // The exception handler then ignores system exceptions like invalid addresses
    // and null pointers. This class can be used to remove this block and prevent
    // it from catching the exception. Typically this will cause the exception to crash 
    // which is often desirable to allow crashlogs to be recorded for debugging purposed.
    // While this class is in scope we replace the Windows exception handler with a custom
    // handler that indicates exceptions that should not be handled.
    //
    // See http://www.microsoft.com/msj/0197/Exception/Exception.aspx,
    //     http://www.microsoft.com/msj/archive/S2CE.aspx
    //     http://www.hexblog.com/wp-content/uploads/2012/06/Recon-2012-Skochinsky-Compiler-Internals.pdf
    //     http://www.codeproject.com/Articles/2126/How-a-C-compiler-implements-exception-handling

    // Windows doesn't like assigning to member variables, so we need to get the value into
    // a local variable and store it afterwards.
    void* registration;

    // Note: The FS register on Windows always holds the Thread Information Block.
    // FS:[0] points to the structured exception handling chain (a chain of
    // EXCEPTION_REGISTRATION structs).
    //
    // struct EXCEPTION_REGISTRATION
    // {
    //     DWORD next;
    //     DWORD handler;
    // };
    //
    // The first four bytes of FS:[0] point to the 'Next' member in the chain. Grab it so we can restore it later.
    __asm mov eax, FS:[0]
    __asm mov [registration], eax

    exceptionRegistration.prev = (ExceptionRegistration*)registration;
    exceptionRegistration.handler = (void*)exceptionHandlerThunk;

    void* erStructMem = &exceptionRegistration;

    __asm mov eax, erStructMem
    __asm mov FS:[0], eax

    m_savedExceptionRegistration = registration;
}

StructuredExceptionHandlerSuppressor::~StructuredExceptionHandlerSuppressor()
{
    // Restore the exception handler
    __asm mov eax, [m_savedExceptionRegistration]
    __asm mov FS:[0], eax
}

#pragma warning(pop)

}
