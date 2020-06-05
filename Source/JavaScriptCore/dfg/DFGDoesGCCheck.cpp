/*
 * Copyright (C) 2020 Apple Inc. All rights reserved.
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
#include "DFGDoesGCCheck.h"

#include "CallFrameInlines.h"
#include "CodeBlock.h"
#include "DFGNodeType.h"
#include "Heap.h"
#include "Options.h"
#include "VMInspector.h"
#include <wtf/DataLog.h>

namespace JSC {
namespace DFG {

#if ENABLE(DFG_DOES_GC_VALIDATION)

extern const char* dfgOpNames[];

void DoesGCCheck::verifyCanGC(VM& vm)
{
    // We do this check here just so we don't have to #include DFGNodeType.h
    // in the header file.
    static_assert(numberOfNodeTypes <= (1 << nodeOpBits));

    if (!Options::validateDoesGC())
        return;

    if (!expectDoesGC()) {
        dataLog("Error: DoesGC failed");
        if (isSpecial()) {
            switch (special()) {
            case Special::Uninitialized:
                break;
            case Special::DFGOSRExit:
                dataLog(" @ DFG osr exit");
                break;
            case Special::FTLOSRExit:
                dataLog(" @ FTL osr exit");
                break;
            case Special::NumberOfSpecials:
                RELEASE_ASSERT_NOT_REACHED();
            }
        } else
            dataLog(" @ D@", nodeIndex(), " ", DFG::dfgOpNames[nodeOp()]);

        CallFrame* callFrame = vm.topCallFrame;
        if (callFrame) {
            CodeBlock* codeBlock = callFrame->codeBlock();
            dataLogLn(" in ", codeBlock);
            VMInspector::dumpStack(&vm, callFrame);
        }
        dataLogLn();
    }
    RELEASE_ASSERT(expectDoesGC());
}
#endif // ENABLE(DFG_DOES_GC_VALIDATION)

} // namespace DFG
} // namespace JSC

