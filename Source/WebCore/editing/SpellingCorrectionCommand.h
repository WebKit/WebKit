/*
 * Copyright (C) 2011 Apple Inc. All rights reserved.
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

#include "CompositeEditCommand.h"
#include "Range.h"

namespace WebCore {

class SpellingCorrectionCommand : public CompositeEditCommand {
public:
    static Ref<SpellingCorrectionCommand> create(Range& rangeToBeCorrected, const String& correction)
    {
        return adoptRef(*new SpellingCorrectionCommand(rangeToBeCorrected, correction));
    }
private:
    SpellingCorrectionCommand(Range& rangeToBeCorrected, const String& correction);
    bool willApplyCommand() final;
    void doApply() override;
    bool shouldRetainAutocorrectionIndicator() const override;

    String inputEventData() const final;
    Vector<RefPtr<StaticRange>> targetRanges() const final;
    RefPtr<DataTransfer> inputEventDataTransfer() const final;

    Ref<Range> m_rangeToBeCorrected;
    VisibleSelection m_selectionToBeCorrected;
    RefPtr<DocumentFragment> m_correctionFragment;
    String m_corrected;
    String m_correction;
};

} // namespace WebCore
