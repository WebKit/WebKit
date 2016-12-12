/*
 * Copyright (C) 2010, 2012, 2013 Apple Inc. All rights reserved.
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

#include "CodeSpecializationKind.h"
#include "JITEntryPoints.h"
#include "ThunkGenerator.h"

#if ENABLE(JIT)
namespace JSC {

class CallLinkInfo;

MacroAssemblerCodeRef throwExceptionFromCallSlowPathGenerator(VM*);

MacroAssemblerCodeRef linkCallThunk(VM*, CallLinkInfo&, CodeSpecializationKind);
JITJSCallThunkEntryPointsWithRef linkCallThunkGenerator(VM*);
JITJSCallThunkEntryPointsWithRef linkDirectCallThunkGenerator(VM*);
JITJSCallThunkEntryPointsWithRef linkPolymorphicCallThunkGenerator(VM*);

JITJSCallThunkEntryPointsWithRef virtualThunkFor(VM*, CallLinkInfo&);

JITEntryPointsWithRef nativeCallGenerator(VM*);
JITEntryPointsWithRef nativeConstructGenerator(VM*);
MacroAssemblerCodeRef nativeTailCallGenerator(VM*);
MacroAssemblerCodeRef nativeTailCallWithoutSavedTagsGenerator(VM*);
MacroAssemblerCodeRef arityFixupGenerator(VM*);
MacroAssemblerCodeRef unreachableGenerator(VM*);

JITEntryPointsWithRef charCodeAtThunkGenerator(VM*);
JITEntryPointsWithRef charAtThunkGenerator(VM*);
JITEntryPointsWithRef clz32ThunkGenerator(VM*);
JITEntryPointsWithRef fromCharCodeThunkGenerator(VM*);
JITEntryPointsWithRef absThunkGenerator(VM*);
JITEntryPointsWithRef ceilThunkGenerator(VM*);
JITEntryPointsWithRef expThunkGenerator(VM*);
JITEntryPointsWithRef floorThunkGenerator(VM*);
JITEntryPointsWithRef logThunkGenerator(VM*);
JITEntryPointsWithRef roundThunkGenerator(VM*);
JITEntryPointsWithRef sqrtThunkGenerator(VM*);
JITEntryPointsWithRef imulThunkGenerator(VM*);
JITEntryPointsWithRef randomThunkGenerator(VM*);
JITEntryPointsWithRef truncThunkGenerator(VM*);

JITEntryPointsWithRef boundThisNoArgsFunctionCallGenerator(VM*);

}
#endif // ENABLE(JIT)
