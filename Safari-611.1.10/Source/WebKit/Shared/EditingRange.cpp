/*
 * Copyright (C) 2019 Apple Inc. All rights reserved.
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
#include "EditingRange.h"

#include <WebCore/Frame.h>
#include <WebCore/FrameSelection.h>
#include <WebCore/TextIterator.h>
#include <WebCore/VisibleUnits.h>

namespace WebKit {

Optional<WebCore::SimpleRange> EditingRange::toRange(WebCore::Frame& frame, const EditingRange& editingRange, EditingRangeIsRelativeTo base)
{
    ASSERT(editingRange.location != notFound);
    WebCore::CharacterRange range { editingRange.location, editingRange.length };

    if (base == EditingRangeIsRelativeTo::EditableRoot) {
        // Our critical assumption is that this code path is called by input methods that
        // concentrate on a given area containing the selection.
        // We have to do this because of text fields and textareas. The DOM for those is not
        // directly in the document DOM, so serialization is problematic. Our solution is
        // to use the root editable element of the selection start as the positional base.
        // That fits with AppKit's idea of an input context.
        auto* element = frame.selection().rootEditableElementOrDocumentElement();
        if (!element)
            return WTF::nullopt;
        return resolveCharacterRange(makeRangeSelectingNodeContents(*element), range);
    }

    ASSERT(base == EditingRangeIsRelativeTo::Paragraph);

    auto paragraphStart = makeBoundaryPoint(startOfParagraph(frame.selection().selection().visibleStart()));
    if (!paragraphStart)
        return WTF::nullopt;

    auto scopeEnd = makeBoundaryPointAfterNodeContents(paragraphStart->container->treeScope().rootNode());
    return WebCore::resolveCharacterRange({ WTFMove(*paragraphStart), WTFMove(scopeEnd) }, range);
}

EditingRange EditingRange::fromRange(WebCore::Frame& frame, const Optional<WebCore::SimpleRange>& range, EditingRangeIsRelativeTo editingRangeIsRelativeTo)
{
    ASSERT(editingRangeIsRelativeTo == EditingRangeIsRelativeTo::EditableRoot);

    if (!range)
        return { };

    auto* element = frame.selection().rootEditableElementOrDocumentElement();
    if (!element)
        return { };

    auto relativeRange = characterRange(makeBoundaryPointBeforeNodeContents(*element), *range);
    return EditingRange(relativeRange.location, relativeRange.length);
}

} // namespace WebKit

namespace IPC {

void ArgumentCoder<WebKit::EditingRange>::encode(Encoder& encoder, const WebKit::EditingRange& editingRange)
{
    encoder << editingRange.location;
    encoder << editingRange.length;
}

Optional<WebKit::EditingRange> ArgumentCoder<WebKit::EditingRange>::decode(Decoder& decoder)
{
    WebKit::EditingRange editingRange;

    if (!decoder.decode(editingRange.location))
        return WTF::nullopt;
    if (!decoder.decode(editingRange.length))
        return WTF::nullopt;

    return editingRange;
}

} // namespace IPC
