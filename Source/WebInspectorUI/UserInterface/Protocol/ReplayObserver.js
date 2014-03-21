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

WebInspector.ReplayPosition = function(segmentOffset, inputOffset)
{
    this.segmentOffset = segmentOffset;
    this.inputOffset = inputOffset;
};

WebInspector.ReplayPosition.fromProtocol = function(payload)
{
    return new WebInspector.ReplayPosition(payload.segmentOffset, payload.inputOffset);
}

WebInspector.ReplayObserver = function()
{
    WebInspector.Object.call(this);
};

WebInspector.ReplayObserver.prototype = {
    constructor: WebInspector.ReplayObserver,
    __proto__: WebInspector.Object.prototype,

    captureStarted: function()
    {
        WebInspector.replayManager.captureStarted();
    },

    captureStopped: function()
    {
        WebInspector.replayManager.captureStopped();
    },

    playbackStarted: function()
    {
        WebInspector.replayManager.playbackStarted();
    },

    playbackHitPosition: function(replayPosition, timestamp)
    {
        WebInspector.replayManager.playbackHitPosition(WebInspector.ReplayPosition.fromProtocol(replayPosition), timestamp);
    },

    playbackPaused: function(replayPosition)
    {
        WebInspector.replayManager.playbackPaused(WebInspector.ReplayPosition.fromProtocol(replayPosition));
    },

    playbackFinished: function()
    {
        WebInspector.replayManager.playbackFinished();
    },

    inputSuppressionChanged: function(willSuppress)
    {
        // Not handled yet.
    },

    sessionCreated: function(sessionId)
    {
        WebInspector.replayManager.sessionCreated(sessionId);
    },

    sessionModified: function(sessionId)
    {
        WebInspector.replayManager.sessionModified(sessionId);
    },

    sessionRemoved: function(sessionId)
    {
        WebInspector.replayManager.sessionRemoved(sessionId);
    },

    sessionLoaded: function(sessionId)
    {
        WebInspector.replayManager.sessionLoaded(sessionId);
    },

    segmentCreated: function(segmentId)
    {
        WebInspector.replayManager.segmentCreated(segmentId);
    },

    segmentRemoved: function(segmentId)
    {
        WebInspector.replayManager.segmentRemoved(segmentId);
    },

    segmentCompleted: function(segmentId)
    {
        WebInspector.replayManager.segmentCompleted(segmentId);
    },

    segmentLoaded: function(segmentId)
    {
        WebInspector.replayManager.segmentLoaded(segmentId);
    },

    segmentUnloaded: function()
    {
        WebInspector.replayManager.segmentUnloaded();
    }
};
