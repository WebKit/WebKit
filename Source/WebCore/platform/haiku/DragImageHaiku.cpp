/*
 * Copyright (C) 2007 Apple Inc.  All rights reserved.
 * Copyright (C) 2007 Ryan Leavengood <leavengood@gmail.com>
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
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#include "config.h"
#include "DragImage.h"

#include "CachedImage.h"
#include "FontCascade.h"
#include "Image.h"

#include "NotImplemented.h"


namespace WebCore {

const float DragLabelBorderX = 4;
// Keep border_y in synch with DragController::LinkDragBorderInset.
const float DragLabelBorderY = 2;
const float DragLabelRadius = 5;
const float LabelBorderYOffset = 2;

const float MinDragLabelWidthBeforeClip = 120;
const float MaxDragLabelWidth = 200;
const float MaxDragLabelStringWidth = (MaxDragLabelWidth - 2 * DragLabelBorderX);

const float DragLinkLabelFontsize = 11;
const float DragLinkUrlFontSize = 10;

static FontCascade dragLabelFont(int size, bool bold, FontRenderingMode renderingMode)
{
    FontCascade result;

    FontCascadeDescription description;
    description.setWeight(bold ? boldWeightValue() : normalWeightValue());
    //description.setOneFamily(metrics.lfSmCaptionFont.lfFaceName);
    description.setSpecifiedSize((float)size);
    description.setComputedSize((float)size);
    description.setRenderingMode(renderingMode);
    result = FontCascade(WTFMove(description), 0, 0);
    result.update(0);
    return result;
}


DragImageRef createDragImageForColor(const Color&, const FloatRect&, float, Path&)
{
    return nullptr;
}

} // namespace WebCore

