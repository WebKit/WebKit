/*
 * Copyright (C) 2010 Apple Inc. All rights reserved.
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

#ifndef DrawingAreaMessageKinds_h
#define DrawingAreaMessageKinds_h

#include "MessageID.h"

// Messages sent from the web process to the UI process.

namespace DrawingAreaLegacyMessage {

enum Kind {
    // Called whenever the size of the drawing area needs to be updated.
    SetSize,

    // Called when the drawing area should stop painting.
    SuspendPainting,

    // Called when the drawing area should start painting again.
    ResumePainting,

    // Called when an update chunk sent to the drawing area has been
    // incorporated into the backing store.
    DidUpdate,

#if ENABLE(TILED_BACKING_STORE)
    // Called to request a tile update.
    RequestTileUpdate,

    // Called to cancel a requested tile update.
    CancelTileUpdate,

    // Called to take a snapshot of the page contents.
    TakeSnapshot,
#endif
};

}

namespace CoreIPC {

template<> struct MessageKindTraits<DrawingAreaLegacyMessage::Kind> { 
    static const MessageClass messageClass = MessageClassDrawingAreaLegacy;
};

}

#endif // DrawingAreaMessageKinds_h
