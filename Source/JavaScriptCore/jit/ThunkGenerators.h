/*
 * Copyright (C) 2010-2021 Apple Inc. All rights reserved.
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

#pragma once

#if ENABLE(JIT)

#include "CallMode.h"
#include "CodeSpecializationKind.h"
#include "JSCPtrTag.h"
#include "ThunkGenerator.h"

namespace JSC {

class CallLinkInfo;
enum class CallMode;
template<PtrTag> class MacroAssemblerCodeRef;
class VM;

MacroAssemblerCodeRef<JITThunkPtrTag> handleExceptionGenerator(VM&, IncludeDebuggerHook);
MacroAssemblerCodeRef<JITThunkPtrTag> handleExceptionWithCallFrameRollbackGenerator(VM&, IncludeDebuggerHook);
MacroAssemblerCodeRef<JITThunkPtrTag> popThunkStackPreservesAndHandleExceptionGenerator(VM&, IncludeDebuggerHook);

MacroAssemblerCodeRef<JITThunkPtrTag> throwExceptionFromCallSlowPathGenerator(VM&, IncludeDebuggerHook);

MacroAssemblerCodeRef<JITThunkPtrTag> linkCallThunkGenerator(VM&, IncludeDebuggerHook);
MacroAssemblerCodeRef<JITThunkPtrTag> linkPolymorphicCallThunkGenerator(VM&, IncludeDebuggerHook);

MacroAssemblerCodeRef<JITStubRoutinePtrTag> virtualThunkFor(VM&, CallMode);

MacroAssemblerCodeRef<JITThunkPtrTag> nativeCallGenerator(VM&, IncludeDebuggerHook);
MacroAssemblerCodeRef<JITThunkPtrTag> nativeConstructGenerator(VM&, IncludeDebuggerHook);
MacroAssemblerCodeRef<JITThunkPtrTag> nativeTailCallGenerator(VM&, IncludeDebuggerHook);
MacroAssemblerCodeRef<JITThunkPtrTag> nativeTailCallWithoutSavedTagsGenerator(VM&, IncludeDebuggerHook);
MacroAssemblerCodeRef<JITThunkPtrTag> internalFunctionCallGenerator(VM&, IncludeDebuggerHook);
MacroAssemblerCodeRef<JITThunkPtrTag> internalFunctionConstructGenerator(VM&, IncludeDebuggerHook);
MacroAssemblerCodeRef<JITThunkPtrTag> arityFixupGenerator(VM&, IncludeDebuggerHook);
MacroAssemblerCodeRef<JITThunkPtrTag> unreachableGenerator(VM&, IncludeDebuggerHook);
MacroAssemblerCodeRef<JITThunkPtrTag> stringGetByValGenerator(VM&, IncludeDebuggerHook);

MacroAssemblerCodeRef<JITThunkPtrTag> charCodeAtThunkGenerator(VM&, IncludeDebuggerHook);
MacroAssemblerCodeRef<JITThunkPtrTag> charAtThunkGenerator(VM&, IncludeDebuggerHook);
MacroAssemblerCodeRef<JITThunkPtrTag> stringPrototypeCodePointAtThunkGenerator(VM&, IncludeDebuggerHook);
MacroAssemblerCodeRef<JITThunkPtrTag> clz32ThunkGenerator(VM&, IncludeDebuggerHook);
MacroAssemblerCodeRef<JITThunkPtrTag> fromCharCodeThunkGenerator(VM&, IncludeDebuggerHook);
MacroAssemblerCodeRef<JITThunkPtrTag> absThunkGenerator(VM&, IncludeDebuggerHook);
MacroAssemblerCodeRef<JITThunkPtrTag> ceilThunkGenerator(VM&, IncludeDebuggerHook);
MacroAssemblerCodeRef<JITThunkPtrTag> expThunkGenerator(VM&, IncludeDebuggerHook);
MacroAssemblerCodeRef<JITThunkPtrTag> floorThunkGenerator(VM&, IncludeDebuggerHook);
MacroAssemblerCodeRef<JITThunkPtrTag> logThunkGenerator(VM&, IncludeDebuggerHook);
MacroAssemblerCodeRef<JITThunkPtrTag> roundThunkGenerator(VM&, IncludeDebuggerHook);
MacroAssemblerCodeRef<JITThunkPtrTag> sqrtThunkGenerator(VM&, IncludeDebuggerHook);
MacroAssemblerCodeRef<JITThunkPtrTag> imulThunkGenerator(VM&, IncludeDebuggerHook);
MacroAssemblerCodeRef<JITThunkPtrTag> randomThunkGenerator(VM&, IncludeDebuggerHook);
MacroAssemblerCodeRef<JITThunkPtrTag> truncThunkGenerator(VM&, IncludeDebuggerHook);

MacroAssemblerCodeRef<JITThunkPtrTag> boundFunctionCallGenerator(VM&, IncludeDebuggerHook);

MacroAssemblerCodeRef<JITThunkPtrTag> remoteFunctionCallGenerator(VM&, IncludeDebuggerHook);

MacroAssemblerCodeRef<JITThunkPtrTag> checkExceptionGenerator(VM&, IncludeDebuggerHook);

} // namespace JSC
#endif // ENABLE(JIT)
