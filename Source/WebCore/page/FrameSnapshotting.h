/*
 *  Copyright (C) 2013 University of Washington. All rights reserved.
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

#ifndef FrameSnapshotting_h
#define FrameSnapshotting_h

#include <wtf/Forward.h>

namespace WebCore {

class Frame;
class IntRect;
class ImageBuffer;
class Node;

enum {
    SnapshotOptionsNone = 0,
    SnapshotOptionsExcludeSelectionHighlighting = 1 << 0,
    SnapshotOptionsInViewCoordinates = 1 << 1,
    SnapshotOptionsForceBlackText = 1 << 2,
};
typedef uint32_t SnapshotOptions;

std::unique_ptr<ImageBuffer> snapshotFrameRect(Frame&, const IntRect&, SnapshotOptions = SnapshotOptionsNone);
std::unique_ptr<ImageBuffer> snapshotNode(Frame&, Node&);
std::unique_ptr<ImageBuffer> snapshotSelection(Frame&, SnapshotOptions = SnapshotOptionsNone);

} // namespace WebCore

#endif // FrameSnapshotting_h
