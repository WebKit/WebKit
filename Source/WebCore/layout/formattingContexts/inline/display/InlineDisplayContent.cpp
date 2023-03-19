/*
 * Copyright (C) 2023 Apple Inc. All rights reserved.
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
#include "InlineDisplayContent.h"

#include "TextPainter.h"


namespace WebCore {
namespace InlineDisplay {

static void invalidateGlyphCache(Boxes& boxes, size_t firstBoxIndex, size_t numberOfBoxes)
{
    ASSERT(firstBoxIndex + numberOfBoxes <= boxes.size());
    for (size_t index = 0; index < numberOfBoxes; ++index)
        TextPainter::removeGlyphDisplayList(boxes[firstBoxIndex + index]);
}

static void invalidateGlyphCache(Boxes& boxes)
{
    invalidateGlyphCache(boxes, 0, boxes.size());
}

void Content::clear()
{
    lines.clear();
    invalidateGlyphCache(boxes);
    boxes.clear();
}

void Content::set(Content&& newContent)
{
    lines = WTFMove(newContent.lines);
    invalidateGlyphCache(boxes);
    boxes = WTFMove(newContent.boxes);
}

void Content::append(Content&& newContent)
{
    lines.appendVector(WTFMove(newContent.lines));
    boxes.appendVector(WTFMove(newContent.boxes));
}

void Content::insert(Content&& newContent, size_t lineIndex, size_t boxIndex)
{
    lines.insertVector(lineIndex, WTFMove(newContent.lines));
    boxes.insertVector(boxIndex, WTFMove(newContent.boxes));
}

void Content::remove(size_t firstLineIndex, size_t numberOfLines, size_t firstBoxIndex, size_t numberOfBoxes)
{
    lines.remove(firstLineIndex, numberOfLines);
    invalidateGlyphCache(boxes, firstBoxIndex, numberOfBoxes);
    boxes.remove(firstBoxIndex, numberOfBoxes);
}

}
}

