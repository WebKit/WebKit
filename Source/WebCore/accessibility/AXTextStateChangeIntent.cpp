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
#include "AXTextStateChangeIntent.h"

#include <wtf/text/MakeString.h>

namespace WebCore {

String debugDescription(AXTextStateChangeType type)
{
    switch (type) {
    case AXTextStateChangeType::Unknown:
        return "Unknown"_s;
    case AXTextStateChangeType::Edit:
        return "Edit"_s;
    case AXTextStateChangeType::SelectionMove:
        return "SelectionMove"_s;
    case AXTextStateChangeType::SelectionExtend:
        return "SelectionExtend"_s;
    case AXTextStateChangeType::SelectionBoundary:
        return "SelectionBoundary"_s;
    }
    return "Unknown"_s;
}

String debugDescription(AXTextEditType type)
{
    switch (type) {
    case AXTextEditType::Unknown:
        return "Unknown"_s;
    case AXTextEditType::Delete:
        return "Delete"_s;
    case AXTextEditType::Insert:
        return "Insert"_s;
    case AXTextEditType::Typing:
        return "Typing"_s;
    case AXTextEditType::Dictation:
        return "Dictation"_s;
    case AXTextEditType::Cut:
        return "Cut"_s;
    case AXTextEditType::Paste:
        return "Paste"_s;
    case AXTextEditType::Replace:
        return "Replace"_s;
    case AXTextEditType::AttributesChange:
        return "AttributesChange"_s;
    }
    return "Unknown"_s;
}

String debugDescription(AXTextSelectionDirection direction)
{
    switch (direction) {
    case AXTextSelectionDirection::Unknown:
        return "Unknown"_s;
    case AXTextSelectionDirection::Beginning:
        return "Beginning"_s;
    case AXTextSelectionDirection::End:
        return "End"_s;
    case AXTextSelectionDirection::Previous:
        return "Previous"_s;
    case AXTextSelectionDirection::Next:
        return "Next"_s;
    case AXTextSelectionDirection::Discontiguous:
        return "Discontiguous"_s;
    }
    return "Unknown"_s;
}

String debugDescription(AXTextSelectionGranularity granularity)
{
    switch (granularity) {
    case AXTextSelectionGranularity::Unknown:
        return "Unknown"_s;
    case AXTextSelectionGranularity::Character:
        return "Character"_s;
    case AXTextSelectionGranularity::Word:
        return "Word"_s;
    case AXTextSelectionGranularity::Line:
        return "Line"_s;
    case AXTextSelectionGranularity::Sentence:
        return "Sentence"_s;
    case AXTextSelectionGranularity::Paragraph:
        return "Paragraph"_s;
    case AXTextSelectionGranularity::Page:
        return "Page"_s;
    case AXTextSelectionGranularity::Document:
        return "Document"_s;
    case AXTextSelectionGranularity::All:
        return "All"_s;
    }
    return "Unknown"_s;
}

String AXTextSelection::debugDescription() const
{
    return makeString("AXTextSelection {direction: "_s, WebCore::debugDescription(direction), ", granularity: "_s, WebCore::debugDescription(granularity), ", focusChange: "_s, focusChange ? "true"_s : "false"_s, '}');
}

String AXTextStateChangeIntent::debugDescription() const
{
    if (type == AXTextStateChangeType::Edit)
        return makeString("AXTextStateChangeIntent {type: "_s, WebCore::debugDescription(type), ", editType: "_s, WebCore::debugDescription(editType), '}');
    return makeString("AXTextStateChangeIntent {type: "_s, WebCore::debugDescription(type), ", selection: "_s, selection.debugDescription(), '}');
}

} // namespace WebCore
