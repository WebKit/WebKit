/*
 * Copyright (C) 2013-2018 Apple Inc. All rights reserved.
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
#include "DFGOSRExitBase.h"

#if ENABLE(DFG_JIT)

#include "InlineCallFrame.h"
#include <wtf/LEBDecoder.h>
#include <wtf/LEBEncoder.h>
#include <wtf/UnalignedAccess.h>

namespace JSC { namespace DFG {

void OSRExitBase::considerAddingAsFrequentExitSite(CodeBlock* profiledCodeBlock, ExitingJITType jitType)
{
    if (!exitKindMayJettison(m_kind))
        return;

    CodeBlock* sourceProfiledCodeBlock =
        baselineCodeBlockForOriginAndBaselineCodeBlock(
            m_codeOriginForExitProfile, profiledCodeBlock);
    if (sourceProfiledCodeBlock) {
        ExitingInlineKind inlineKind;
        if (m_codeOriginForExitProfile.inlineCallFrame())
            inlineKind = ExitFromInlined;
        else
            inlineKind = ExitFromNotInlined;
        
        FrequentExitSite site;
        if (m_wasHoisted)
            site = FrequentExitSite(HoistingFailed, jitType, inlineKind);
        else
            site = FrequentExitSite(m_codeOriginForExitProfile.bytecodeIndex(), m_kind, jitType, inlineKind);
        ExitProfile::add(sourceProfiledCodeBlock, site);
    }
}

CodeOriginTag codeOriginTag(const CodeOrigin& codeOrigin, const CodeOrigin& previous)
{
    if (codeOrigin == previous)
        return CodeOriginTag::SameAsPrevious;
    if (codeOrigin.inlineCallFrame() == previous.inlineCallFrame())
        return CodeOriginTag::SameInlineCallFrame;
    return CodeOriginTag::NewInlineCallFrame;
}

void encodeCodeOrigin(Vector<uint8_t>& bytes, CodeOriginTag tag, const CodeOrigin& codeOrigin, const CodeOrigin& previous)
{
    switch (tag) {
    case CodeOriginTag::SameAsPrevious:
        return;
    case CodeOriginTag::NewInlineCallFrame: {
        InlineCallFrame* inlineCallFrame = codeOrigin.inlineCallFrame();
        bytes.append(asByteSpan(inlineCallFrame));
        [[fallthrough]];
    }
    case CodeOriginTag::SameInlineCallFrame:
        WTF::LEBEncoder::encodeInt32(bytes, static_cast<int32_t>(codeOrigin.bytecodeIndex().asBits() - previous.bytecodeIndex().asBits()));
        return;
    }
}

CodeOrigin decodeCodeOrigin(std::span<const uint8_t> bytes, size_t& offset, CodeOriginTag tag, const CodeOrigin& previous)
{
    InlineCallFrame* inlineCallFrame = previous.inlineCallFrame();
    switch (tag) {
    case CodeOriginTag::SameAsPrevious:
        return previous;
    case CodeOriginTag::NewInlineCallFrame:
        inlineCallFrame = WTF::unalignedLoad<InlineCallFrame*>(bytes.subspan(offset, sizeof(InlineCallFrame*)).data());
        offset += sizeof(InlineCallFrame*);
        [[fallthrough]];
    case CodeOriginTag::SameInlineCallFrame:
        return CodeOrigin(BytecodeIndex::fromBits(previous.bytecodeIndex().asBits() + WTF::LEBDecoder::decodeInt32OrCrash(bytes, offset)), inlineCallFrame);
    }
    RELEASE_ASSERT_NOT_REACHED();
}

} } // namespace JSC::DFG

#endif // ENABLE(DFG_JIT)

