/*
 * Copyright (C) 2013 University of Washington. All rights reserved.
 * Copyright (C) 2014 Apple Inc. All rights reserved.
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

// FIXME: This ReplayPosition class shouldn't be here, no matter how simple it is.
WebInspector.ReplayPosition = class ReplayPosition
{
    constructor(segmentOffset, inputOffset)
    {
        this.segmentOffset = segmentOffset;
        this.inputOffset = inputOffset;
    }
};

WebInspector.ReplayObserver = class ReplayObserver
{
    // Events defined by the "Replay" domain.

    captureStarted()
    {
        WebInspector.replayManager.captureStarted();
    }

    captureStopped()
    {
        WebInspector.replayManager.captureStopped();
    }

    playbackStarted()
    {
        WebInspector.replayManager.playbackStarted();
    }

    playbackHitPosition(replayPosition, timestamp)
    {
        WebInspector.replayManager.playbackHitPosition(new WebInspector.ReplayPosition(replayPosition.segmentOffset, replayPosition.inputOffset), timestamp);
    }

    playbackPaused(replayPosition)
    {
        WebInspector.replayManager.playbackPaused(new WebInspector.ReplayPosition(replayPosition.segmentOffset, replayPosition.inputOffset));
    }

    playbackFinished()
    {
        WebInspector.replayManager.playbackFinished();
    }

    inputSuppressionChanged(willSuppress)
    {
        // Not handled yet.
    }

    sessionCreated(sessionId)
    {
        WebInspector.replayManager.sessionCreated(sessionId);
    }

    sessionModified(sessionId)
    {
        WebInspector.replayManager.sessionModified(sessionId);
    }

    sessionRemoved(sessionId)
    {
        WebInspector.replayManager.sessionRemoved(sessionId);
    }

    sessionLoaded(sessionId)
    {
        WebInspector.replayManager.sessionLoaded(sessionId);
    }

    segmentCreated(segmentId)
    {
        WebInspector.replayManager.segmentCreated(segmentId);
    }

    segmentRemoved(segmentId)
    {
        WebInspector.replayManager.segmentRemoved(segmentId);
    }

    segmentCompleted(segmentId)
    {
        WebInspector.replayManager.segmentCompleted(segmentId);
    }

    segmentLoaded(segmentId)
    {
        WebInspector.replayManager.segmentLoaded(segmentId);
    }

    segmentUnloaded()
    {
        WebInspector.replayManager.segmentUnloaded();
    }
};
