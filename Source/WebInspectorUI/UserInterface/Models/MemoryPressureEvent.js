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

WebInspector.MemoryPressureEvent = class MemoryPressureEvent extends WebInspector.Object
{
    constructor(timestamp, severity)
    {
        super();

        this._timestamp = timestamp;
        this._severity = severity;
    }

    // Static

    static fromPayload(timestamp, protocolSeverity)
    {
        let severity;
        switch (protocolSeverity) {
        case MemoryAgent.MemoryPressureSeverity.Critical:
            severity = WebInspector.MemoryPressureEvent.Severity.Critical;
            break;
        case MemoryAgent.MemoryPressureSeverity.NonCritical:
            severity = WebInspector.MemoryPressureEvent.Severity.NonCritical;
            break;
        default:
            console.error("Unexpected memory pressure severity", protocolSeverity);
            severity = WebInspector.MemoryPressureEvent.Severity.NonCritical;
            break;
        }

        return new WebInspector.MemoryPressureEvent(timestamp, severity);
    }

    // Public

    get timestamp() { return this._timestamp; }
    get severity() { return this._severity; }
};

WebInspector.MemoryPressureEvent.Severity = {
    Critical: Symbol("Critical"),
    NonCritical: Symbol("NonCritical"),
};
