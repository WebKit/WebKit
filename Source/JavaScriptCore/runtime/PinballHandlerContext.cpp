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

#include "config.h"
#include "PinballHandlerContext.h"

#if ENABLE(WEBASSEMBLY)

#include "CallFrame.h"
#include "JSCellInlines.h"
#include "JSFunctionWithFields.h"
#include "JSPIContextInlines.h"
#include "PinballCompletion.h"
#include "StackAlignment.h"

#include <wtf/StdLibExtras.h>

namespace JSC {

PinballHandlerContext::PinballHandlerContext(JSGlobalObject* globalObject, CallFrame* callFrame)
    : globalObject(globalObject)
    , vm(&globalObject->vm())
    , handler(uncheckedDowncast<JSFunctionWithFields>(callFrame->jsCallee()))
    , pinball(uncheckedDowncast<PinballCompletion>(handler->getField(JSFunctionWithFields::Field::PromiseHandlerPinballCompletion)))
    , sliceByteSize(pinball->topSlice()->size() * sizeof(Register))
    , jspiContext(JSPIContext::Purpose::Completing, *vm, callFrame, pinball->resultPromise())
    , evacuatedCalleeSaves(pinball->calleeSaves())
{
    ASSERT(pinball->hasSlices());
    ASSERT(!(sliceByteSize % stackAlignmentBytes()));
#if ASSERT_ENABLED
    zeroSpan(std::span(arguments));
#endif
}

} // namespace JSC

#endif // ENABLE(WEBASSEMBLY)
