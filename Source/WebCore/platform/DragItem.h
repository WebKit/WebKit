/*
 * Copyright (C) 2017 Apple Inc. All rights reserved.
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

#include "DragActions.h"
#include "DragImage.h"
#include "FloatPoint.h"
#include "IntPoint.h"
#include "IntRect.h"
#include "PasteboardWriterData.h"
#include "PromisedAttachmentInfo.h"

namespace WebCore {

struct DragItem final {
    DragImage image;

    // Where the image should be positioned relative to the cursor.
    FloatPoint imageAnchorPoint;

    Optional<DragSourceAction> sourceAction;
    IntPoint eventPositionInContentCoordinates;
    IntPoint dragLocationInContentCoordinates;
    IntPoint dragLocationInWindowCoordinates;
    String title;
    URL url;
    IntRect dragPreviewFrameInRootViewCoordinates;

    PasteboardWriterData data;
    PromisedAttachmentInfo promisedAttachmentInfo;

    template<class Encoder> void encode(Encoder&) const;
    template<class Decoder> static WARN_UNUSED_RETURN bool decode(Decoder&, DragItem&);
};

template<class Encoder>
void DragItem::encode(Encoder& encoder) const
{
    // FIXME(173815): We should encode and decode PasteboardWriterData and platform drag image data
    // here too, as part of moving off of the legacy dragging codepath.
    encoder << sourceAction;
    encoder << imageAnchorPoint << eventPositionInContentCoordinates << dragLocationInContentCoordinates << dragLocationInWindowCoordinates << title << url << dragPreviewFrameInRootViewCoordinates;
    bool hasIndicatorData = image.hasIndicatorData();
    encoder << hasIndicatorData;
    if (hasIndicatorData)
        encoder << image.indicatorData().value();
    bool hasVisiblePath = image.hasVisiblePath();
    encoder << hasVisiblePath;
    if (hasVisiblePath)
        encoder << image.visiblePath().value();
    encoder << promisedAttachmentInfo;
}

template<class Decoder>
bool DragItem::decode(Decoder& decoder, DragItem& result)
{
    if (!decoder.decode(result.sourceAction))
        return false;
    if (!decoder.decode(result.imageAnchorPoint))
        return false;
    if (!decoder.decode(result.eventPositionInContentCoordinates))
        return false;
    if (!decoder.decode(result.dragLocationInContentCoordinates))
        return false;
    if (!decoder.decode(result.dragLocationInWindowCoordinates))
        return false;
    if (!decoder.decode(result.title))
        return false;
    if (!decoder.decode(result.url))
        return false;
    if (!decoder.decode(result.dragPreviewFrameInRootViewCoordinates))
        return false;
    bool hasIndicatorData;
    if (!decoder.decode(hasIndicatorData))
        return false;
    if (hasIndicatorData) {
        Optional<TextIndicatorData> indicatorData;
        decoder >> indicatorData;
        if (!indicatorData)
            return false;
        result.image.setIndicatorData(*indicatorData);
    }
    bool hasVisiblePath;
    if (!decoder.decode(hasVisiblePath))
        return false;
    if (hasVisiblePath) {
        Optional<Path> visiblePath;
        decoder >> visiblePath;
        if (!visiblePath)
            return false;
        result.image.setVisiblePath(*visiblePath);
    }
    if (!decoder.decode(result.promisedAttachmentInfo))
        return false;
    return true;
}

}
