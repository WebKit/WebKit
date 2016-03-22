/*
 * Copyright (C) 2015 Apple Inc. All rights reserved.
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

WebInspector.Instrument = class Instrument extends WebInspector.Object
{
    // Static

    static createForTimelineType(type)
    {
        switch (type) {
        case WebInspector.TimelineRecord.Type.Network:
            return new WebInspector.NetworkInstrument;
        case WebInspector.TimelineRecord.Type.Layout:
            return new WebInspector.LayoutInstrument;
        case WebInspector.TimelineRecord.Type.Script:
            return new WebInspector.ScriptInstrument;
        case WebInspector.TimelineRecord.Type.RenderingFrame:
            return new WebInspector.FPSInstrument;
        case WebInspector.TimelineRecord.Type.Memory:
            return new WebInspector.MemoryInstrument;
        case WebInspector.TimelineRecord.Type.HeapAllocations:
            return new WebInspector.HeapAllocationsInstrument;
        default:
            console.error("Unknown TimelineRecord.Type: " + type);
            return null;
        }
    }

    static startLegacyTimelineAgent()
    {
        console.assert(window.TimelineAgent, "Attempted to start legacy timeline agent without TimelineAgent.");

        if (WebInspector.Instrument._legacyTimelineAgentStarted)
            return;

        WebInspector.Instrument._legacyTimelineAgentStarted = true;

        let result = TimelineAgent.start();

        // COMPATIBILITY (iOS 7): recordingStarted event did not exist yet. Start explicitly.
        if (!TimelineAgent.hasEvent("recordingStarted")) {
            result.then(function() {
                WebInspector.timelineManager.capturingStarted();
            });
        }
    }

    static stopLegacyTimelineAgent()
    {
        if (!WebInspector.Instrument._legacyTimelineAgentStarted)
            return;

        WebInspector.Instrument._legacyTimelineAgentStarted = false;

        TimelineAgent.stop();
    }

    // Protected

    get timelineRecordType()
    {
        return null; // Implemented by subclasses.
    }

    startInstrumentation()
    {
        WebInspector.Instrument.startLegacyTimelineAgent();
    }

    stopInstrumentation()
    {
        WebInspector.Instrument.stopLegacyTimelineAgent();
    }
};

WebInspector.Instrument._legacyTimelineAgentStarted = false;
