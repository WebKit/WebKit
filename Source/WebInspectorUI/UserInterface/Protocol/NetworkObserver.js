/*
 * Copyright (C) 2013, 2015 Apple Inc. All rights reserved.
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

WI.NetworkObserver = class NetworkObserver
{
    // Events defined by the "Network" domain.

    requestWillBeSent(requestId, frameId, loaderId, documentURL, request, timestamp, walltime, initiator, redirectResponse, type, targetId)
    {
        // COMPATIBILITY(iOS 11.0): `walltime` did not exist in 11.0 and earlier.
        if (!NetworkAgent.hasEventParameter("requestWillBeSent", "walltime")) {
            walltime = undefined;
            initiator = arguments[6];
            redirectResponse = arguments[7];
            type = arguments[8];
            targetId = arguments[9];
        }

        WI.frameResourceManager.resourceRequestWillBeSent(requestId, frameId, loaderId, request, type, redirectResponse, timestamp, walltime, initiator, targetId);
    }

    requestServedFromCache(requestId)
    {
        // COMPATIBILITY (iOS 10.3): The backend no longer sends this.
        WI.frameResourceManager.markResourceRequestAsServedFromMemoryCache(requestId);
    }

    responseReceived(requestId, frameId, loaderId, timestamp, type, response)
    {
        WI.frameResourceManager.resourceRequestDidReceiveResponse(requestId, frameId, loaderId, type, response, timestamp);
    }

    dataReceived(requestId, timestamp, dataLength, encodedDataLength)
    {
        WI.frameResourceManager.resourceRequestDidReceiveData(requestId, dataLength, encodedDataLength, timestamp);
    }

    loadingFinished(requestId, timestamp, sourceMapURL, metrics)
    {
        WI.frameResourceManager.resourceRequestDidFinishLoading(requestId, timestamp, sourceMapURL, metrics);
    }

    loadingFailed(requestId, timestamp, errorText, canceled)
    {
        WI.frameResourceManager.resourceRequestDidFailLoading(requestId, canceled, timestamp, errorText);
    }

    requestServedFromMemoryCache(requestId, frameId, loaderId, documentURL, timestamp, initiator, resource)
    {
        WI.frameResourceManager.resourceRequestWasServedFromMemoryCache(requestId, frameId, loaderId, resource, timestamp, initiator);
    }

    webSocketCreated(requestId, url)
    {
        WI.frameResourceManager.webSocketCreated(requestId, url);
    }

    webSocketWillSendHandshakeRequest(requestId, timestamp, walltime, request)
    {
        WI.frameResourceManager.webSocketWillSendHandshakeRequest(requestId, timestamp, walltime, request);
    }

    webSocketHandshakeResponseReceived(requestId, timestamp, response)
    {
        WI.frameResourceManager.webSocketHandshakeResponseReceived(requestId, timestamp, response);
    }

    webSocketClosed(requestId, timestamp)
    {
        WI.frameResourceManager.webSocketClosed(requestId, timestamp);
    }

    webSocketFrameReceived(requestId, timestamp, response)
    {
        WI.frameResourceManager.webSocketFrameReceived(requestId, timestamp, response);
    }

    webSocketFrameSent(requestId, timestamp, response)
    {
        WI.frameResourceManager.webSocketFrameSent(requestId, timestamp, response);
    }

    webSocketFrameError(requestId, timestamp, errorMessage)
    {
        // FIXME: Not implemented.
    }
};
