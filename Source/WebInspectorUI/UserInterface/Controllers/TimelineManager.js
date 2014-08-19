/*
 * Copyright (C) 2013 Apple Inc. All rights reserved.
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

WebInspector.TimelineManager = function()
{
    WebInspector.Object.call(this);

    WebInspector.Frame.addEventListener(WebInspector.Frame.Event.ProvisionalLoadStarted, this._startAutoCapturing, this);
    WebInspector.Frame.addEventListener(WebInspector.Frame.Event.MainResourceDidChange, this._mainResourceDidChange, this);
    WebInspector.Frame.addEventListener(WebInspector.Frame.Event.ResourceWasAdded, this._resourceWasAdded, this);

    this._activeRecording = new WebInspector.TimelineRecording;
    this._isCapturing = false;

    this._boundStopCapturing = this.stopCapturing.bind(this);
};

WebInspector.TimelineManager.Event = {
    CapturingStarted: "timeline-manager-capturing-started",
    CapturingStopped: "timeline-manager-capturing-stopped"
};

WebInspector.TimelineManager.MaximumAutoRecordDuration = 90000; // 90 seconds
WebInspector.TimelineManager.MaximumAutoRecordDurationAfterLoadEvent = 10000; // 10 seconds
WebInspector.TimelineManager.DeadTimeRequiredToStopAutoRecordingEarly = 2000; // 2 seconds

WebInspector.TimelineManager.prototype = {
    constructor: WebInspector.TimelineManager,

    // Public

    get activeRecording()
    {
        return this._activeRecording;
    },

    isCapturing: function()
    {
        return this._isCapturing;
    },

    startCapturing: function()
    {
        TimelineAgent.start();

        // COMPATIBILITY (iOS 7): recordingStarted event did not exist yet. Start explicitly.
        if (!TimelineAgent.hasEvent("recordingStarted"))
            this.capturingStarted();
    },

    stopCapturing: function()
    {
        TimelineAgent.stop();

        // NOTE: Always stop immediately instead of waiting for a Timeline.recordingStopped event.
        // This way the UI feels as responsive to a stop as possible.
        this.capturingStopped();
    },

    capturingStarted: function()
    {
        if (this._isCapturing)
            return;

        this._isCapturing = true;

        this.dispatchEventToListeners(WebInspector.TimelineManager.Event.CapturingStarted);
    },

    capturingStopped: function()
    {
        if (!this._isCapturing)
            return;

        if (this._stopCapturingTimeout) {
            clearTimeout(this._stopCapturingTimeout);
            delete this._stopCapturingTimeout;
        }

        if (this._deadTimeTimeout) {
            clearTimeout(this._deadTimeTimeout);
            delete this._deadTimeTimeout;
        }

        this._isCapturing = false;
        this._autoCapturingMainResource = null;

        this.dispatchEventToListeners(WebInspector.TimelineManager.Event.CapturingStopped);
    },

    eventRecorded: function(originalRecordPayload)
    {
        // Called from WebInspector.TimelineObserver.

        if (!this._isCapturing)
            return;

        function processRecord(recordPayload, parentRecordPayload)
        {
            // Convert the timestamps to seconds to match the resource timestamps.
            var startTime = recordPayload.startTime / 1000;
            var endTime = recordPayload.endTime / 1000;

            var callFrames = this._callFramesFromPayload(recordPayload.stackTrace);

            var significantCallFrame = null;
            if (callFrames) {
                for (var i = 0; i < callFrames.length; ++i) {
                    if (callFrames[i].nativeCode)
                        continue;
                    significantCallFrame = callFrames[i];
                    break;
                }
            }

            var sourceCodeLocation = significantCallFrame && significantCallFrame.sourceCodeLocation;

            switch (recordPayload.type) {
            case TimelineAgent.EventType.MarkLoad:
                console.assert(isNaN(endTime));

                var frame = WebInspector.frameResourceManager.frameForIdentifier(recordPayload.frameId);
                console.assert(frame);
                if (!frame)
                    break;

                frame.markLoadEvent(startTime);

                if (!frame.isMainFrame())
                    break;

                var eventMarker = new WebInspector.TimelineMarker(startTime, WebInspector.TimelineMarker.Type.LoadEvent);
                this._activeRecording.addEventMarker(eventMarker);

                this._stopAutoRecordingSoon();
                break;

            case TimelineAgent.EventType.MarkDOMContent:
                console.assert(isNaN(endTime));

                var frame = WebInspector.frameResourceManager.frameForIdentifier(recordPayload.frameId);
                console.assert(frame);
                if (!frame)
                    break;

                frame.markDOMContentReadyEvent(startTime);

                if (!frame.isMainFrame())
                    break;

                var eventMarker = new WebInspector.TimelineMarker(startTime, WebInspector.TimelineMarker.Type.DOMContentEvent);
                this._activeRecording.addEventMarker(eventMarker);
                break;

            case TimelineAgent.EventType.ScheduleStyleRecalculation:
                console.assert(isNaN(endTime));

                // Pass the startTime as the endTime since this record type has no duration.
                this._addRecord(new WebInspector.LayoutTimelineRecord(WebInspector.LayoutTimelineRecord.EventType.InvalidateStyles, startTime, startTime, callFrames, sourceCodeLocation));
                break;

            case TimelineAgent.EventType.RecalculateStyles:
                this._addRecord(new WebInspector.LayoutTimelineRecord(WebInspector.LayoutTimelineRecord.EventType.RecalculateStyles, startTime, endTime, callFrames, sourceCodeLocation));
                break;

            case TimelineAgent.EventType.InvalidateLayout:
                console.assert(isNaN(endTime));

                // Pass the startTime as the endTime since this record type has no duration.
                this._addRecord(new WebInspector.LayoutTimelineRecord(WebInspector.LayoutTimelineRecord.EventType.InvalidateLayout, startTime, startTime, callFrames, sourceCodeLocation));
                break;

            case TimelineAgent.EventType.Layout:
                var layoutRecordType = sourceCodeLocation ? WebInspector.LayoutTimelineRecord.EventType.ForcedLayout : WebInspector.LayoutTimelineRecord.EventType.Layout;

                // COMPATIBILITY (iOS 6): Layout records did not contain area properties. This is not exposed via a quad "root".
                var quad = recordPayload.data.root ? new WebInspector.Quad(recordPayload.data.root) : null;
                if (quad)
                    this._addRecord(new WebInspector.LayoutTimelineRecord(layoutRecordType, startTime, endTime, callFrames, sourceCodeLocation, quad.points[0].x, quad.points[0].y, quad.width, quad.height, quad));
                else
                    this._addRecord(new WebInspector.LayoutTimelineRecord(layoutRecordType, startTime, endTime, callFrames, sourceCodeLocation));
                break;

            case TimelineAgent.EventType.Paint:
                // COMPATIBILITY (iOS 6): Paint records data contained x, y, width, height properties. This became a quad "clip".
                var quad = recordPayload.data.clip ? new WebInspector.Quad(recordPayload.data.clip) : null;
                if (quad)
                    this._addRecord(new WebInspector.LayoutTimelineRecord(WebInspector.LayoutTimelineRecord.EventType.Paint, startTime, endTime, callFrames, sourceCodeLocation, null, null, quad.width, quad.height, quad));
                else
                    this._addRecord(new WebInspector.LayoutTimelineRecord(WebInspector.LayoutTimelineRecord.EventType.Paint, startTime, endTime, callFrames, sourceCodeLocation, recordPayload.data.x, recordPayload.data.y, recordPayload.data.width, recordPayload.data.height));
                break;

            case TimelineAgent.EventType.EvaluateScript:
                if (!sourceCodeLocation) {
                    var mainFrame = WebInspector.frameResourceManager.mainFrame;
                    var scriptResource = mainFrame.url === recordPayload.data.url ? mainFrame.mainResource : mainFrame.resourceForURL(recordPayload.data.url, true);
                    if (scriptResource) {
                        // The lineNumber is 1-based, but we expect 0-based.
                        var lineNumber = recordPayload.data.lineNumber - 1;

                        // FIXME: No column number is provided.
                        sourceCodeLocation = scriptResource.createSourceCodeLocation(lineNumber, 0);
                    }
                }

                var profileData = recordPayload.data.profile;

                switch (parentRecordPayload && parentRecordPayload.type) {
                case TimelineAgent.EventType.TimerFire:
                    this._addRecord(new WebInspector.ScriptTimelineRecord(WebInspector.ScriptTimelineRecord.EventType.TimerFired, startTime, endTime, callFrames, sourceCodeLocation, parentRecordPayload.data.timerId, profileData));
                    break;
                default:
                    this._addRecord(new WebInspector.ScriptTimelineRecord(WebInspector.ScriptTimelineRecord.EventType.ScriptEvaluated, startTime, endTime, callFrames, sourceCodeLocation, null, profileData));
                    break;
                }

                break;

            case TimelineAgent.EventType.TimeStamp:
                var eventMarker = new WebInspector.TimelineMarker(startTime, WebInspector.TimelineMarker.Type.TimeStamp);
                this._activeRecording.addEventMarker(eventMarker);
                break;

            case TimelineAgent.EventType.ConsoleProfile:
                var profileData = recordPayload.data.profile;
                console.assert(profileData);
                this._addRecord(new WebInspector.ScriptTimelineRecord(WebInspector.ScriptTimelineRecord.EventType.ConsoleProfileRecorded, startTime, endTime, callFrames, sourceCodeLocation, recordPayload.data.title, profileData));
                break;

            case TimelineAgent.EventType.FunctionCall:
                // FunctionCall always happens as a child of another record, and since the FunctionCall record
                // has useful info we just make the timeline record here (combining the data from both records).
                if (!parentRecordPayload)
                    break;

                if (!sourceCodeLocation) {
                    var mainFrame = WebInspector.frameResourceManager.mainFrame;
                    var scriptResource = mainFrame.url === recordPayload.data.scriptName ? mainFrame.mainResource : mainFrame.resourceForURL(recordPayload.data.scriptName, true);
                    if (scriptResource) {
                        // The lineNumber is 1-based, but we expect 0-based.
                        var lineNumber = recordPayload.data.scriptLine - 1;

                        // FIXME: No column number is provided.
                        sourceCodeLocation = scriptResource.createSourceCodeLocation(lineNumber, 0);
                    }
                }

                var profileData = recordPayload.data.profile;

                switch (parentRecordPayload.type) {
                case TimelineAgent.EventType.TimerFire:
                    this._addRecord(new WebInspector.ScriptTimelineRecord(WebInspector.ScriptTimelineRecord.EventType.TimerFired, startTime, endTime, callFrames, sourceCodeLocation, parentRecordPayload.data.timerId, profileData));
                    break;
                case TimelineAgent.EventType.EventDispatch:
                    this._addRecord(new WebInspector.ScriptTimelineRecord(WebInspector.ScriptTimelineRecord.EventType.EventDispatched, startTime, endTime, callFrames, sourceCodeLocation, parentRecordPayload.data.type, profileData));
                    break;
                case TimelineAgent.EventType.XHRLoad:
                    this._addRecord(new WebInspector.ScriptTimelineRecord(WebInspector.ScriptTimelineRecord.EventType.EventDispatched, startTime, endTime, callFrames, sourceCodeLocation, "load", profileData));
                    break;
                case TimelineAgent.EventType.XHRReadyStateChange:
                    this._addRecord(new WebInspector.ScriptTimelineRecord(WebInspector.ScriptTimelineRecord.EventType.EventDispatched, startTime, endTime, callFrames, sourceCodeLocation, "readystatechange", profileData));
                    break;
                case TimelineAgent.EventType.FireAnimationFrame:
                    this._addRecord(new WebInspector.ScriptTimelineRecord(WebInspector.ScriptTimelineRecord.EventType.AnimationFrameFired, startTime, endTime, callFrames, sourceCodeLocation, parentRecordPayload.data.id, profileData));
                    break;
                }

                break;

            case TimelineAgent.EventType.ProbeSample:
                // Pass the startTime as the endTime since this record type has no duration.
                sourceCodeLocation = WebInspector.probeManager.probeForIdentifier(recordPayload.data.probeId).breakpoint.sourceCodeLocation;
                this._addRecord(new WebInspector.ScriptTimelineRecord(WebInspector.ScriptTimelineRecord.EventType.ProbeSampleRecorded, startTime, startTime, callFrames, sourceCodeLocation, recordPayload.data.probeId));
                break;

            case TimelineAgent.EventType.TimerInstall:
                console.assert(isNaN(endTime));

                // Pass the startTime as the endTime since this record type has no duration.
                this._addRecord(new WebInspector.ScriptTimelineRecord(WebInspector.ScriptTimelineRecord.EventType.TimerInstalled, startTime, startTime, callFrames, sourceCodeLocation, recordPayload.data.timerId));
                break;

            case TimelineAgent.EventType.TimerRemove:
                console.assert(isNaN(endTime));

                // Pass the startTime as the endTime since this record type has no duration.
                this._addRecord(new WebInspector.ScriptTimelineRecord(WebInspector.ScriptTimelineRecord.EventType.TimerRemoved, startTime, startTime, callFrames, sourceCodeLocation, recordPayload.data.timerId));
                break;

            case TimelineAgent.EventType.RequestAnimationFrame:
                console.assert(isNaN(endTime));

                // Pass the startTime as the endTime since this record type has no duration.
                this._addRecord(new WebInspector.ScriptTimelineRecord(WebInspector.ScriptTimelineRecord.EventType.AnimationFrameRequested, startTime, startTime, callFrames, sourceCodeLocation, recordPayload.data.timerId));
                break;

            case TimelineAgent.EventType.CancelAnimationFrame:
                console.assert(isNaN(endTime));

                // Pass the startTime as the endTime since this record type has no duration.
                this._addRecord(new WebInspector.ScriptTimelineRecord(WebInspector.ScriptTimelineRecord.EventType.AnimationFrameCanceled, startTime, startTime, callFrames, sourceCodeLocation, recordPayload.data.timerId));
                break;
            }
        }

        // Iterate over the records tree using a stack. Doing this recursively has
        // been known to cause a call stack overflow. https://webkit.org/b/79106
        var stack = [{array: [originalRecordPayload], parent: null, index: 0}];
        while (stack.length) {
            var entry = stack.lastValue;
            var recordPayloads = entry.array;
            var parentRecordPayload = entry.parent;

            if (entry.index < recordPayloads.length) {
                var recordPayload = recordPayloads[entry.index];

                processRecord.call(this, recordPayload, parentRecordPayload);

                if (recordPayload.children)
                    stack.push({array: recordPayload.children, parent: recordPayload, index: 0});
                ++entry.index;
            } else
                stack.pop();
        }
    },

    pageDidLoad: function(timestamp)
    {
        if (isNaN(WebInspector.frameResourceManager.mainFrame.loadEventTimestamp))
            WebInspector.frameResourceManager.mainFrame.markLoadEvent(timestamp);
    },

    // Private

    _callFramesFromPayload: function(payload)
    {
        if (!payload)
            return null;

        function createCallFrame(payload)
        {
            var url = payload.url;
            var nativeCode = false;

            if (url === "[native code]") {
                nativeCode = true;
                url = null;
            }

            var sourceCode = WebInspector.frameResourceManager.resourceForURL(url);
            if (!sourceCode)
                sourceCode = WebInspector.debuggerManager.scriptsForURL(url)[0];

            // The lineNumber is 1-based, but we expect 0-based.
            var lineNumber = payload.lineNumber - 1;

            var sourceCodeLocation = sourceCode ? sourceCode.createLazySourceCodeLocation(lineNumber, payload.columnNumber) : null;
            var functionName = payload.functionName !== "global code" ? payload.functionName : null;

            return new WebInspector.CallFrame(null, sourceCodeLocation, functionName, null, null, nativeCode);
        }

        return payload.map(createCallFrame);
    },

    _addRecord: function(record)
    {
        this._activeRecording.addRecord(record);

        // Only worry about dead time after the load event.
        if (!isNaN(WebInspector.frameResourceManager.mainFrame.loadEventTimestamp))
            this._resetAutoRecordingDeadTimeTimeout();
    },

    _startAutoCapturing: function(event)
    {
        if (!event.target.isMainFrame() || (this._isCapturing && !this._autoCapturingMainResource))
            return false;

        var mainResource = event.target.provisionalMainResource || event.target.mainResource;
        if (mainResource === this._autoCapturingMainResource)
            return false;

        this.stopCapturing();

        this._autoCapturingMainResource = mainResource;

        this._activeRecording.reset();

        this.startCapturing();

        this._addRecord(new WebInspector.ResourceTimelineRecord(mainResource));

        if (this._stopCapturingTimeout)
            clearTimeout(this._stopCapturingTimeout);
        this._stopCapturingTimeout = setTimeout(this._boundStopCapturing, WebInspector.TimelineManager.MaximumAutoRecordDuration);

        return true;
    },

    _stopAutoRecordingSoon: function()
    {
        // Only auto stop when auto capturing.
        if (!this._isCapturing || !this._autoCapturingMainResource)
            return;

        if (this._stopCapturingTimeout)
            clearTimeout(this._stopCapturingTimeout);
        this._stopCapturingTimeout = setTimeout(this._boundStopCapturing, WebInspector.TimelineManager.MaximumAutoRecordDurationAfterLoadEvent);
    },

    _resetAutoRecordingDeadTimeTimeout: function()
    {
        // Only monitor dead time when auto capturing.
        if (!this._isCapturing || !this._autoCapturingMainResource)
            return;

        if (this._deadTimeTimeout)
            clearTimeout(this._deadTimeTimeout);
        this._deadTimeTimeout = setTimeout(this._boundStopCapturing, WebInspector.TimelineManager.DeadTimeRequiredToStopAutoRecordingEarly);
    },

    _mainResourceDidChange: function(event)
    {
        // Ignore resource events when there isn't a main frame yet. Those events are triggered by
        // loading the cached resources when the inspector opens, and they do not have timing information.
        if (!WebInspector.frameResourceManager.mainFrame)
            return;

        if (this._startAutoCapturing(event))
            return;

        if (!this._isCapturing)
            return;

        var mainResource = event.target.mainResource;
        if (mainResource === this._autoCapturingMainResource)
            return;

        this._addRecord(new WebInspector.ResourceTimelineRecord(mainResource));
    },

    _resourceWasAdded: function(event)
    {
        // Ignore resource events when there isn't a main frame yet. Those events are triggered by
        // loading the cached resources when the inspector opens, and they do not have timing information.
        if (!WebInspector.frameResourceManager.mainFrame)
            return;

        if (!this._isCapturing)
            return;

        this._addRecord(new WebInspector.ResourceTimelineRecord(event.data.resource));
    }
};

WebInspector.TimelineManager.prototype.__proto__ = WebInspector.Object.prototype;
