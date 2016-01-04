/*
 * Copyright (C) 2016 Apple Inc. All rights reserved.
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

WebInspector.CapturingProtocolTracer = class CapturingProtocolTracer extends WebInspector.ProtocolTracer
{
    constructor()
    {
        super();

        this._trace = new WebInspector.ProtocolTrace;
    }

    // Public

    get trace()
    {
        return this._trace;
    }

    logFrontendException(message, exception)
    {
        this._processEntry({type: "exception", message: this._stringifyMessage(message), exception});
    }

    logProtocolError(message, error)
    {
        this._processEntry({type: "error", message: this._stringifyMessage(message), error});
    }

    logFrontendRequest(message)
    {
        this._processEntry({type: "request", message: this._stringifyMessage(message)});
    }

    logDidHandleResponse(message, timings = null)
    {
        let entry = {type: "response", message: this._stringifyMessage(message)};
        if (timings)
            entry.timings = Object.shallowCopy(timings);

        this._processEntry(entry);
    }

    logDidHandleEvent(message, timings = null)
    {
        let entry = {type: "event", message: this._stringifyMessage(message)};
        if (timings)
            entry.timings = Object.shallowCopy(timings);

        this._processEntry(entry);
    }

    _stringifyMessage(message)
    {
        try {
            return JSON.stringify(message);
        } catch (e) {
            console.error("couldn't stringify object:", message, e);
            return {};
        }
    }

    _processEntry(entry)
    {
        this._trace.addEntry(entry);
    }
};
