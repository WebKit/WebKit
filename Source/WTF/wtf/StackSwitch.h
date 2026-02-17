/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
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

#include <wtf/Assertions.h>
#include <wtf/ExportMacros.h>
#include <wtf/Platform.h>

// List of all stack-switching trampolines.
// Each trampoline switches from the current stack to a new stack and calls a target function.
// This is used to "teleport" to a sequestered stack after thread startup or other special contexts.
//
// Macro parameters:
//   trampolineName: Name of the trampoline function (e.g., callThreadEntryPointFinishSetupWithNewStack)
//   argType: Type of the context argument (e.g., void*, Heap*)
//   argName: Parameter name for the context argument (e.g., context, heap)
//   callExpression: Expression to invoke the target function (e.g., Thread::entryPointFinishSetup(context), heap->method())
//
// Trampoline signature: void trampolineName(argType argName, void* newStack)
//   - argName: Context argument passed to the target function (or 'this' pointer for member functions)
//   - newStack: Pointer to the top of the new stack (stacks grow down)
//
// When the function returns, the trampoline restores the original stack and returns to the caller.
#define WTF_FOR_EACH_STACK_SWITCH_TRAMPOLINE(macro) \
    macro(callThreadEntryPointFinishSetupWithNewStack, void*, context, Thread::entryPointFinishSetup(context))

extern "C" {

#define DECLARE_STACK_SWITCH_TRAMPOLINE(trampolineName, argType, argName, callExpression) \
    WTF_EXPORT_PRIVATE void trampolineName(argType argName, void* newStack);

WTF_FOR_EACH_STACK_SWITCH_TRAMPOLINE(DECLARE_STACK_SWITCH_TRAMPOLINE)
#undef DECLARE_STACK_SWITCH_TRAMPOLINE

}
