/*
 * Copyright (C) 2017-2019 Apple Inc. All rights reserved.
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
 * PURPOSE ARE DISCLAIMED. IN NOEVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

// LocalResource is a complete resource constructed by the frontend. It
// does not need to be tied to an object in the backend. The local resource
// does not need to be complete at creation and can get its content later.
//
// Construction values try to mimic protocol inputs to WI.Resource:
//
//     request: { url, method, headers, timestamp, walltime, finishedTimestamp data }
//     response: { mimeType, headers, statusCode, statusText, failureReasonText, content, base64Encoded }
//     metrics: { responseSource, protocol, priority, remoteAddress, connectionIdentifier, sizes }
//     timing: { startTime, domainLookupStart, domainLookupEnd, connectStart, connectEnd, secureConnectionStart, requestStart, responseStart, responseEnd }
//     isLocalResourceOverride: <boolean>

WI.LocalResource = class LocalResource extends WI.Resource
{
    constructor({request, response, metrics, timing, isLocalResourceOverride})
    {
        console.assert(request);
        console.assert(typeof request.url === "string");
        console.assert(response);

        metrics = metrics || {};
        timing = timing || {};

        super(request.url, {
            mimeType: response.mimeType || (response.headers || {}).valueForCaseInsensitiveKey("Content-Type") || null,
            requestMethod: request.method,
            requestHeaders: request.headers,
            requestData: request.data,
            requestSentTimestamp: request.timestamp,
            requestSentWalltime: request.walltime,
        });

        // NOTE: This directly overwrites WI.Resource properties.
        this._finishedOrFailedTimestamp = request.finishedTimestamp || NaN;
        this._statusCode = response.statusCode || NaN;
        this._statusText = response.statusText || null;
        this._responseHeaders = response.headers || {};
        this._failureReasonText = response.failureReasonText;
        this._timingData = new WI.ResourceTimingData(this, timing);

        this._responseSource = metrics.responseSource || WI.Resource.ResponseSource.Unknown;
        this._protocol = metrics.protocol || null;
        this._priority = metrics.priority || WI.Resource.NetworkPriority.Unknown;
        this._remoteAddress = metrics.remoteAddress || null;
        this._connectionIdentifier = metrics.connectionIdentifier || null;
        this._requestHeadersTransferSize = !isNaN(metrics.requestHeaderBytesSent) ? metrics.requestHeaderBytesSent : NaN;
        this._requestBodyTransferSize = !isNaN(metrics.requestBodyBytesSent) ? metrics.requestBodyBytesSent : NaN;
        this._responseHeadersTransferSize = !isNaN(metrics.responseHeaderBytesReceived) ? metrics.responseHeaderBytesReceived : NaN;
        this._responseBodyTransferSize = !isNaN(metrics.responseBodyBytesReceived) ? metrics.responseBodyBytesReceived : NaN;
        this._responseBodySize = !isNaN(metrics.responseBodyDecodedSize) ? metrics.responseBodyDecodedSize : NaN;

        // LocalResource specific.
        this._isLocalResourceOverride = isLocalResourceOverride || false;

        // Finalize WI.Resource.
        this._finished = true;
        this._failed = false; // FIXME: How should we denote a failure? Assume from status code / failure reason?
        this._cached = false; // FIXME: How should we denote cached? Assume from response source?

        // Finalize WI.SourceCode.
        let content = response.content;
        let base64Encoded = response.base64Encoded;
        this._originalRevision = new WI.SourceCodeRevision(this, content, base64Encoded, this._mimeType);
        this._currentRevision = this._originalRevision;
    }

    // Static

    static headersArrayToHeadersObject(headers)
    {
        let result = {};
        if (headers) {
            for (let {name, value} of headers)
                result[name] = value;
        }
        return result;
    }

    static fromHAREntry(entry, archiveStartWalltime)
    {
        // FIXME: <https://webkit.org/b/195694> Web Inspector: HAR Extension for Redirect Timing Info

        let {request, response, startedDateTime, timings} = entry;
        let requestSentWalltime = WI.HARBuilder.dateFromHARDate(startedDateTime) / 1000;
        let requestSentTimestamp = requestSentWalltime - archiveStartWalltime;
        let finishedTimestamp = NaN;

        let timing = {
            startTime: NaN,
            domainLookupStart: NaN,
            domainLookupEnd: NaN,
            connectStart: NaN,
            connectEnd: NaN,
            secureConnectionStart: NaN,
            requestStart: NaN,
            responseStart: NaN,
            responseEnd: NaN,
        };

        if (!isNaN(requestSentWalltime) && !isNaN(archiveStartWalltime)) {
            let hasBlocked = timings.blocked !== -1;
            let hasDNS = timings.dns !== -1;
            let hasConnect = timings.connect !== -1;
            let hasSecureConnect = timings.ssl !== -1;

            // FIXME: <https://webkit.org/b/195692> Web Inspector: ResourceTimingData should allow a startTime of 0
            timing.startTime = requestSentTimestamp || Number.EPSILON;
            timing.fetchStart = timing.startTime;

            let accumulation = timing.startTime;

            if (hasBlocked)
                accumulation += (timings.blocked / 1000);

            if (hasDNS) {
                timing.domainLookupStart = accumulation;
                accumulation += (timings.dns / 1000);
                timing.domainLookupEnd = accumulation;
            }

            if (hasConnect) {
                timing.connectStart = accumulation;
                accumulation += (timings.connect / 1000);
                timing.connectEnd = accumulation;

                if (hasSecureConnect)
                    timing.secureConnectionStart = timing.connectEnd - (timings.ssl / 1000);
            }

            accumulation += (timings.send / 1000);
            timing.requestStart = accumulation;

            accumulation += (timings.wait / 1000);
            timing.responseStart = accumulation;

            accumulation += (timings.receive / 1000);
            timing.responseEnd = accumulation;

            finishedTimestamp = timing.responseEnd;
        }

        let serverAddress = entry.serverIPAddress || null;
        if (serverAddress && typeof entry._serverPort === "number")
            serverAddress += ":" + entry._serverPort;

        return new WI.LocalResource({
            request: {
                url: request.url,
                method: request.method,
                headers: LocalResource.headersArrayToHeadersObject(request.headers),
                timestamp: requestSentTimestamp,
                walltime: requestSentWalltime,
                finishedTimestamp: finishedTimestamp,
                data: request.postData ? request.postData.text : null,
            },
            response: {
                headers: LocalResource.headersArrayToHeadersObject(response.headers),
                mimeType: response.content.mimeType,
                statusCode: response.status,
                statusText: response.statusText,
                failureReasonText: response._error || null,
                content: response.content.text,
                base64Encoded: response.content.encoding === "base64",
            },
            metrics: {
                responseSource: WI.HARBuilder.responseSourceFromHARFetchType(entry._fetchType),
                protocol: WI.HARBuilder.protocolFromHARProtocol(response.httpVersion),
                priority: WI.HARBuilder.networkPriorityFromHARPriority(entry._priority),
                remoteAddress: serverAddress,
                connectionIdentifier: entry.connection ? parseInt(entry.connection) : null,
                requestHeaderBytesSent: request.headersSize >= 0 ? request.headersSize : NaN,
                requestBodyBytesSent: request.bodySize >= 0 ? request.bodySize : NaN,
                responseHeaderBytesReceived: response.headersSize >= 0 ? response.headersSize : NaN,
                responseBodyBytesReceived: response.bodySize >= 0 ? response.bodySize : NaN,
                responseBodyDecodedSize: response.content.size || NaN,
            },
            timing,
        });
    }

    // Import / Export

    static fromJSON(json)
    {
        return new WI.LocalResource(json);
    }

    toJSON(key)
    {
        return {
            request: {
                url: this.url,
            },
            response: {
                headers: this.responseHeaders,
                mimeType: this.mimeType,
                statusCode: this.statusCode,
                statusText: this.statusText,
                content: this.currentRevision.content,
                base64Encoded: this.currentRevision.base64Encoded,
            },
            isLocalResourceOverride: this._isLocalResourceOverride,
        };
    }

    // Public

    get isLocalResourceOverride()
    {
        return this._isLocalResourceOverride;
    }

    // Protected

    requestContentFromBackend()
    {
        return Promise.resolve({
            content: this._originalRevision.content,
            base64Encoded: this._originalRevision.base64Encoded,
        });
    }

    handleCurrentRevisionContentChange()
    {
        if (this._mimeType !== this.currentRevision.mimeType) {
            let oldMIMEType = this._mimeType;
            this._mimeType = this.currentRevision.mimeType;
            this.dispatchEventToListeners(WI.Resource.Event.MIMETypeDidChange, {oldMIMEType});
        }
    }
};
