/*
 * Copyright (C) 2020-2022 Apple Inc. All rights reserved.
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

#include "config.h"
#include "WebCoreJITOperations.h"

#include <JavaScriptCore/JITOperationList.h>

namespace WebCore {

#if ENABLE(JIT_OPERATION_VALIDATION) || ENABLE(JIT_OPERATION_DISASSEMBLY)

extern const JSC::JITOperationAnnotation startOfJITOperationsInWebCore __asm("section$start$__DATA_CONST$__jsc_ops");
extern const JSC::JITOperationAnnotation endOfJITOperationsInWebCore __asm("section$end$__DATA_CONST$__jsc_ops");

#if ENABLE(JIT_OPERATION_VALIDATION)
void populateJITOperations()
{
    static std::once_flag onceKey;
    std::call_once(onceKey, [] {
        JSC::JITOperationList::populatePointersInEmbedder(&startOfJITOperationsInWebCore, &endOfJITOperationsInWebCore);
    });
#if ENABLE(JIT_OPERATION_DISASSEMBLY)
    if (UNLIKELY(JSC::Options::needDisassemblySupport()))
        populateDisassemblyLabels();
#endif
}
#endif // ENABLE(JIT_OPERATION_VALIDATION)

#if ENABLE(JIT_OPERATION_DISASSEMBLY)
void populateDisassemblyLabels()
{
    static std::once_flag onceKey;
    std::call_once(onceKey, [] {
        JSC::JITOperationList::populateDisassemblyLabelsInEmbedder(&startOfJITOperationsInWebCore, &endOfJITOperationsInWebCore);
    });
}
#endif // ENABLE(JIT_OPERATION_DISASSEMBLY)

#endif // ENABLE(JIT_OPERATION_VALIDATION) || ENABLE(JIT_OPERATION_DISASSEMBLY)

}
