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

#if ENABLE(WRITING_TOOLS)

#include "AttributedString.h"
#include "CharacterRange.h"
#include <wtf/Forward.h>

namespace WebCore {
namespace WritingTools {

enum class ReplacementBehavior : uint8_t {
    None,
    Default,
    Limited,
    Complete,
};

enum class EditAction : uint8_t {
    Undo,
    Redo,
    UndoAll,
};

#pragma mark - Session

enum class SessionReplacementType : uint8_t {
    PlainText,
    RichText,
};

enum class SessionCorrectionType : uint8_t {
    None,
    Grammar,
    Spelling,
};

using SessionID = WTF::UUID;

struct Session {
    using ID = SessionID;

    using ReplacementType = SessionReplacementType;
    using CorrectionType = SessionCorrectionType;

    ID identifier;
    ReplacementType replacementType { ReplacementType::RichText };
    CorrectionType correctionType { CorrectionType::None };
};

#pragma mark - Context

using ContextID = WTF::UUID;

struct Context {
    using ID = ContextID;

    ID identifier;
    AttributedString attributedText;
    CharacterRange range;
};

#pragma mark - Replacement

enum class ReplacementState : uint8_t {
    Pending,
    Active,
    Reverted,
    Invalid,
};

using ReplacementID = WTF::UUID;

struct Replacement {
    using ID = ReplacementID;

    using State = ReplacementState;

    ID identifier;
    CharacterRange originalRange;
    String replacement;
    State state;
};

} // namespace WritingTools
} // namespace WebCore

#endif
