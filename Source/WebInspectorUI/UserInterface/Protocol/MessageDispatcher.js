/*
 * Copyright (C) 2013 Apple Inc. All rights reserved.
 * Copyright (C) 2014 University of Washington
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

WebInspector.messagesToDispatch = [];

WebInspector.dispatchNextQueuedMessageFromBackend = function()
{
    var startCount = WebInspector.messagesToDispatch.length;
    var startTime = Date.now();
    var timeLimitPerRunLoop = 10; // milliseconds

    var i = 0;
    for (; i < WebInspector.messagesToDispatch.length; ++i) {
        // Defer remaining messages if we have taken too long. In practice, single
        // messages like Page.getResourceContent blow through the time budget.
        if (Date.now() - startTime > timeLimitPerRunLoop)
            break;

        InspectorBackend.dispatch(WebInspector.messagesToDispatch[i]);
    }

    if (i === WebInspector.messagesToDispatch.length) {
        WebInspector.messagesToDispatch = [];
        WebInspector._dispatchTimeout = null;
    } else {
        WebInspector.messagesToDispatch = WebInspector.messagesToDispatch.slice(i);
        WebInspector._dispatchTimeout = setTimeout(WebInspector.dispatchNextQueuedMessageFromBackend, 0);
    }

    if (InspectorBackend.dumpInspectorTimeStats)
        console.log("time-stats: --- RunLoop duration: " + (Date.now() - startTime) + "ms; dispatched: " + (startCount - WebInspector.messagesToDispatch.length) + "; remaining: " + WebInspector.messagesToDispatch.length);
}

WebInspector.dispatchMessageFromBackend = function(message)
{
    // Enforce asynchronous interaction between the backend and the frontend by queueing messages.
    // The messages are dequeued on a zero delay timeout.

    this.messagesToDispatch.push(message);

    if (this._dispatchTimeout)
        return;

    this._dispatchTimeout = setTimeout(WebInspector.dispatchNextQueuedMessageFromBackend, 0);
}
