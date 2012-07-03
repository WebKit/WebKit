/*
 * Copyright (C) 2012 Apple Inc. All rights reserved.
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
#include "DFGVariableEvent.h"

#if ENABLE(DFG_JIT)

#include "DFGFPRInfo.h"
#include "DFGGPRInfo.h"

namespace JSC { namespace DFG {

void VariableEvent::dump(FILE* out) const
{
    switch (kind()) {
    case Reset:
        fprintf(out, "Reset");
        break;
    case BirthToFill:
        dumpFillInfo("BirthToFill", out);
        break;
    case BirthToSpill:
        dumpSpillInfo("BirthToSpill", out);
        break;
    case Fill:
        dumpFillInfo("Fill", out);
        break;
    case Spill:
        dumpSpillInfo("Spill", out);
        break;
    case Death:
        fprintf(out, "Death(@%u)", nodeIndex());
        break;
    case MovHint:
        fprintf(out, "MovHint(@%u, r%d)", nodeIndex(), operand());
        break;
    case SetLocalEvent:
        fprintf(out, "SetLocal(r%d, %s)", operand(), dataFormatToString(dataFormat()));
        break;
    default:
        ASSERT_NOT_REACHED();
        break;
    }
}

void VariableEvent::dumpFillInfo(const char* name, FILE* out) const
{
    fprintf(out, "%s(@%u, ", name, nodeIndex());
    if (dataFormat() == DataFormatDouble)
        fprintf(out, "%s", FPRInfo::debugName(fpr()));
#if USE(JSVALUE32_64)
    else if (dataFormat() & DataFormatJS)
        fprintf(out, "%s:%s", GPRInfo::debugName(tagGPR()), GPRInfo::debugName(payloadGPR()));
#endif
    else
        fprintf(out, "%s", GPRInfo::debugName(gpr()));
    fprintf(out, ", %s)", dataFormatToString(dataFormat()));
}

void VariableEvent::dumpSpillInfo(const char* name, FILE* out) const
{
    fprintf(out, "%s(@%u, r%d, %s)", name, nodeIndex(), virtualRegister(), dataFormatToString(dataFormat()));
}

} } // namespace JSC::DFG

#endif // ENABLE(DFG_JIT)

