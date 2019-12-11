/*
 * Copyright (C) 2004, 2005, 2006, 2007, 2008, 2013 Apple Inc. All rights reserved.
 * Copyright (C) 2006 Alexey Proskuryakov (ap@nypop.com)
 * Copyright (C) 2008 Nokia Corporation and/or its subsidiary(-ies)
 * Copyright (C) 2013 University of Washington.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS
 * IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
 * PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include <memory>
#include <wtf/Forward.h>

namespace WebCore {

class FloatRect;
class Frame;
class IntRect;
class ImageBuffer;
class Node;

enum {
    SnapshotOptionsNone = 0,
    SnapshotOptionsExcludeSelectionHighlighting = 1 << 0,
    SnapshotOptionsPaintSelectionOnly = 1 << 1,
    SnapshotOptionsInViewCoordinates = 1 << 2,
    SnapshotOptionsForceBlackText = 1 << 3,
    SnapshotOptionsPaintSelectionAndBackgroundsOnly = 1 << 4,
    SnapshotOptionsPaintEverythingExcludingSelection = 1 << 5,
    SnapshotOptionsPaintWithIntegralScaleFactor = 1 << 6,
};
typedef unsigned SnapshotOptions;

WEBCORE_EXPORT std::unique_ptr<ImageBuffer> snapshotFrameRect(Frame&, const IntRect&, SnapshotOptions = SnapshotOptionsNone);
std::unique_ptr<ImageBuffer> snapshotFrameRectWithClip(Frame&, const IntRect&, const Vector<FloatRect>& clipRects, SnapshotOptions = SnapshotOptionsNone);
std::unique_ptr<ImageBuffer> snapshotNode(Frame&, Node&);
WEBCORE_EXPORT std::unique_ptr<ImageBuffer> snapshotSelection(Frame&, SnapshotOptions = SnapshotOptionsNone);

} // namespace WebCore
