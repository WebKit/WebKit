/*
 * Copyright (C) 2015 Apple Inc. All rights reserved.
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

#include <wtf/Forward.h>

namespace WebCore {

enum class AXTextStateChangeType : uint8_t {
    Unknown,
    Edit,
    SelectionMove,
    SelectionExtend,
    SelectionBoundary
};

enum class AXTextEditType : uint8_t {
    Unknown,
    Delete, // Generic text delete
    Insert, // Generic text insert
    Typing, // Insert via typing
    Dictation, // Insert via dictation
    Cut, // Delete via Cut
    Paste, // Insert via Paste
    Replace, // A deletion + insertion that should be notified as an atomic operation.
    AttributesChange // Change font, style, alignment, color, etc.
};

enum class AXTextSelectionDirection : uint8_t {
    Unknown,
    Beginning,
    End,
    Previous,
    Next,
    Discontiguous
};

enum class AXTextSelectionGranularity : uint8_t {
    Unknown,
    Character,
    Word,
    Line,
    Sentence,
    Paragraph,
    Page,
    Document,
    All // All granularity represents the action of selecting the whole document as a single action. Extending selection by some other granularity until it encompasses the whole document will not result in a all granularity notification.
};

String debugDescription(AXTextStateChangeType);
String debugDescription(AXTextEditType);
String debugDescription(AXTextSelectionDirection);
String debugDescription(AXTextSelectionGranularity);

struct AXTextSelection {
    AXTextSelectionDirection direction { AXTextSelectionDirection::Unknown };
    AXTextSelectionGranularity granularity { AXTextSelectionGranularity::Unknown };
    bool focusChange { false };

    String debugDescription() const;
};

struct AXTextStateChangeIntent {
    AXTextStateChangeType type { AXTextStateChangeType::Unknown };
    union {
        AXTextSelection selection;
        AXTextEditType editType;
    };

    AXTextStateChangeIntent(AXTextStateChangeType type = AXTextStateChangeType::Unknown, AXTextSelection selection = AXTextSelection())
        : type(type)
        , selection(selection)
    { }

    AXTextStateChangeIntent(AXTextEditType editType)
        : type(AXTextStateChangeType::Edit)
        , editType(editType)
    { }

    String debugDescription() const;
};

} // namespace WebCore
