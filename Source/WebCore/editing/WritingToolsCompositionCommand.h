/*
 * Copyright (C) 2024 Apple Inc. All rights reserved.
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

namespace WebCore {

struct AttributedString;

// A Writing Tools Composition command is essentially a wrapper around a group of Replace Selection commands,
// which can be undone/redone as a whole.
//
// It also maintains an associated context range that persists after each constituent command.
class WritingToolsCompositionCommand : public CompositeEditCommand {
public:
    enum class MatchStyle: bool {
        No, Yes
    };

    enum class State: uint8_t {
        InProgress,
        Complete
    };

    static Ref<WritingToolsCompositionCommand> create(Ref<Document>&& document, const SimpleRange& endingContextRange)
    {
        return adoptRef(*new WritingToolsCompositionCommand(WTFMove(document), endingContextRange));
    }

    // This method is used to add each "piece" (aka Replace selection command) of the Writing Tools composition command.
    void replaceContentsOfRangeWithFragment(RefPtr<DocumentFragment>&&, const SimpleRange&, MatchStyle, State);

    SimpleRange endingContextRange() const { return m_endingContextRange; }

    // FIXME: Remove this when WritingToolsController no longer needs to support `contextRangeForSessionWithID`.
    SimpleRange currentContextRange() const { return m_currentContextRange; }

private:
    WritingToolsCompositionCommand(Ref<Document>&&, const SimpleRange&);

    void doApply() override { }

    SimpleRange m_endingContextRange;
    SimpleRange m_currentContextRange;
};

} // namespace WebCore
