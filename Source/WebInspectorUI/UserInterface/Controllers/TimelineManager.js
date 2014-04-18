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

    WebInspector.Frame.addEventListener(WebInspector.Frame.Event.ProvisionalLoadStarted, this._startAutoRecording, this);
    WebInspector.Frame.addEventListener(WebInspector.Frame.Event.MainResourceDidChange, this._mainResourceDidChange, this);
    WebInspector.Frame.addEventListener(WebInspector.Frame.Event.ResourceWasAdded, this._resourceWasAdded, this);

    this._recording = new WebInspector.TimelineRecording;
    this._recordingEnabled = false;
};

WebInspector.TimelineManager.Event = {
    RecordingStarted: "timeline-manager-recording-started",
    RecordingStopped: "timeline-manager-recording-stopped"
};

WebInspector.TimelineManager.MaximumAutoRecordDuration = 90000; // 90 seconds
WebInspector.TimelineManager.MaximumAutoRecordDurationAfterLoadEvent = 10000; // 10 seconds
WebInspector.TimelineManager.DeadTimeRequiredToStopAutoRecordingEarly = 2000; // 2 seconds

WebInspector.TimelineManager.prototype = {
    constructor: WebInspector.TimelineManager,

    // Public

    get recording()
    {
        return this._recording;
    },

    get recordingEnabled()
    {
        return this._recordingEnabled;
    },

    startRecording: function()
    {
        if (this._recordingEnabled)
            return;

        this._recordingEnabled = true;

        TimelineAgent.start();

        this.dispatchEventToListeners(WebInspector.TimelineManager.Event.RecordingStarted);
    },

    stopRecording: function()
    {
        if (!this._recordingEnabled)
            return;

        if (this._stopRecordingTimeout) {
            clearTimeout(this._stopRecordingTimeout);
            delete this._stopRecordingTimeout;
        }

        if (this._deadTimeTimeout) {
            clearTimeout(this._deadTimeTimeout);
            delete this._deadTimeTimeout;
        }

        TimelineAgent.stop();

        this._recordingEnabled = false;
        this._autoRecordingMainResource = null;

        this.dispatchEventToListeners(WebInspector.TimelineManager.Event.RecordingStopped);
    },

    eventRecorded: function(originalRecordPayload)
    {
        // Called from WebInspector.TimelineObserver.

        if (!this._recordingEnabled)
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
                this._recording.addEventMarker(eventMarker);

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
                this._recording.addEventMarker(eventMarker);
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

                var profile = null;
                if (recordPayload.data.profile)
                    profile = this._profileFromPayload(recordPayload.data.profile);

                switch (parentRecordPayload && parentRecordPayload.type) {
                case TimelineAgent.EventType.TimerFire:
                    this._addRecord(new WebInspector.ScriptTimelineRecord(WebInspector.ScriptTimelineRecord.EventType.TimerFired, startTime, endTime, callFrames, sourceCodeLocation, parentRecordPayload.data.timerId, profile));
                    break;
                default:
                    this._addRecord(new WebInspector.ScriptTimelineRecord(WebInspector.ScriptTimelineRecord.EventType.ScriptEvaluated, startTime, endTime, callFrames, sourceCodeLocation, null, profile));
                    break;
                }

                break;

            case TimelineAgent.EventType.TimeStamp:
                var eventMarker = new WebInspector.TimelineMarker(startTime, WebInspector.TimelineMarker.Type.TimeStamp);
                this._recording.addEventMarker(eventMarker);
                break;

            case TimelineAgent.EventType.FunctionCall:
                // FunctionCall always happens as a child of another record, and since the FunctionCall record
                // has useful info we just make the timeline record here (combining the data from both records).
                if (!parentRecordPayload)
                    break;

                var profile = null;
                if (recordPayload.data.profile)
                    profile = this._profileFromPayload(recordPayload.data.profile);

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

                switch (parentRecordPayload.type) {
                case TimelineAgent.EventType.TimerFire:
                    this._addRecord(new WebInspector.ScriptTimelineRecord(WebInspector.ScriptTimelineRecord.EventType.TimerFired, startTime, endTime, callFrames, sourceCodeLocation, parentRecordPayload.data.timerId, profile));
                    break;
                case TimelineAgent.EventType.EventDispatch:
                    this._addRecord(new WebInspector.ScriptTimelineRecord(WebInspector.ScriptTimelineRecord.EventType.EventDispatched, startTime, endTime, callFrames, sourceCodeLocation, parentRecordPayload.data.type, profile));
                    break;
                case TimelineAgent.EventType.XHRLoad:
                    this._addRecord(new WebInspector.ScriptTimelineRecord(WebInspector.ScriptTimelineRecord.EventType.EventDispatched, startTime, endTime, callFrames, sourceCodeLocation, "load", profile));
                    break;
                case TimelineAgent.EventType.XHRReadyStateChange:
                    this._addRecord(new WebInspector.ScriptTimelineRecord(WebInspector.ScriptTimelineRecord.EventType.EventDispatched, startTime, endTime, callFrames, sourceCodeLocation, "readystatechange", profile));
                    break;
                case TimelineAgent.EventType.FireAnimationFrame:
                    this._addRecord(new WebInspector.ScriptTimelineRecord(WebInspector.ScriptTimelineRecord.EventType.AnimationFrameFired, startTime, endTime, callFrames, sourceCodeLocation, parentRecordPayload.data.id, profile));
                    break;
                }

                break;

            case TimelineAgent.EventType.ProbeSample:
                // Pass the startTime as the endTime since this record type has no duration.
                sourceCodeLocation = WebInspector.probeManager.probeForIdentifier(recordPayload.data.probeId).breakpoint.sourceCodeLocation;
                this._addRecord(new WebInspector.ScriptTimelineRecord(WebInspector.ScriptTimelineRecord.EventType.ProbeSampleRecorded, startTime, startTime, callFrames, sourceCodeLocation, recordPayload.data.probeId, profile));
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

    _profileFromPayload: function(payload)
    {
        if (!payload)
            return null;

        console.assert(payload.rootNodes instanceof Array);

        function profileNodeFromPayload(nodePayload)
        {
            console.assert("id" in nodePayload);
            console.assert(nodePayload.calls instanceof Array);

            if (nodePayload.url) {
                var sourceCode = WebInspector.frameResourceManager.resourceForURL(nodePayload.url);
                if (!sourceCode)
                    sourceCode = WebInspector.debuggerManager.scriptsForURL(nodePayload.url)[0];

                // The lineNumber is 1-based, but we expect 0-based.
                var lineNumber = nodePayload.lineNumber - 1;

                var sourceCodeLocation = sourceCode ? sourceCode.createSourceCodeLocation(lineNumber, nodePayload.columnNumber) : null;
            }

            var isProgramCode = nodePayload.functionName === "(program)";
            var isAnonymousFunction = nodePayload.functionName === "(anonymous function)";

            var type = isProgramCode ? WebInspector.ProfileNode.Type.Program : WebInspector.ProfileNode.Type.Function;
            var functionName = !isProgramCode && !isAnonymousFunction && nodePayload.functionName !== "(unknown)" ? nodePayload.functionName : null;
            var calls = nodePayload.calls.map(profileNodeCallFromPayload);

            return new WebInspector.ProfileNode(nodePayload.id, type, functionName, sourceCodeLocation, calls, nodePayload.children);
        }

        function profileNodeCallFromPayload(nodeCallPayload)
        {
            console.assert("startTime" in nodeCallPayload);
            console.assert("totalTime" in nodeCallPayload);

            return new WebInspector.ProfileNodeCall(nodeCallPayload.startTime, nodeCallPayload.totalTime);
        }

        var rootNodes = payload.rootNodes;

        // Iterate over the node tree using a stack. Doing this recursively can easily cause a stack overflow.
        // We traverse the profile in post-order and convert the payloads in place until we get back to the root.
        var stack = [{parent: {children: rootNodes}, index: 0, root: true}];
        while (stack.length) {
            var entry = stack.lastValue;

            if (entry.index < entry.parent.children.length) {
                var childNodePayload = entry.parent.children[entry.index];
                if (childNodePayload.children && childNodePayload.children.length)
                    stack.push({parent: childNodePayload, index: 0});

                ++entry.index;
            } else {
                if (!entry.root)
                    entry.parent.children = entry.parent.children.map(profileNodeFromPayload);
                else
                    rootNodes = rootNodes.map(profileNodeFromPayload);

                stack.pop();
            }
        }

        return new WebInspector.Profile(rootNodes, payload.idleTime);
    },

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

            var sourceCodeLocation = sourceCode ? sourceCode.createSourceCodeLocation(lineNumber, payload.columnNumber) : null;
            var functionName = payload.functionName !== "global code" ? payload.functionName : null;

            return new WebInspector.CallFrame(null, sourceCodeLocation, functionName, null, null, nativeCode);
        }

        return payload.map(createCallFrame);
    },

    _addRecord: function(record)
    {
        this._recording.addRecord(record);

        // Only worry about dead time after the load event.
        if (!isNaN(WebInspector.frameResourceManager.mainFrame.loadEventTimestamp))
            this._resetAutoRecordingDeadTimeTimeout();
    },

    _startAutoRecording: function(event)
    {
        if (!event.target.isMainFrame() || (this._recordingEnabled && !this._autoRecordingMainResource))
            return false;

        var mainResource = event.target.provisionalMainResource || event.target.mainResource;
        if (mainResource === this._autoRecordingMainResource)
            return false;

        this.stopRecording();

        this._autoRecordingMainResource = mainResource;

        this._recording.reset();

        this.startRecording();

        this._addRecord(new WebInspector.ResourceTimelineRecord(mainResource));

        if (this._stopRecordingTimeout)
            clearTimeout(this._stopRecordingTimeout);
        this._stopRecordingTimeout = setTimeout(this.stopRecording.bind(this), WebInspector.TimelineManager.MaximumAutoRecordDuration);

        return true;
    },

    _stopAutoRecordingSoon: function()
    {
        // Only auto stop when auto recording.
        if (!this._recordingEnabled || !this._autoRecordingMainResource)
            return;

        if (this._stopRecordingTimeout)
            clearTimeout(this._stopRecordingTimeout);
        this._stopRecordingTimeout = setTimeout(this.stopRecording.bind(this), WebInspector.TimelineManager.MaximumAutoRecordDurationAfterLoadEvent);
    },

    _resetAutoRecordingDeadTimeTimeout: function()
    {
        // Only monitor dead time when auto recording.
        if (!this._recordingEnabled || !this._autoRecordingMainResource)
            return;

        if (this._deadTimeTimeout)
            clearTimeout(this._deadTimeTimeout);
        this._deadTimeTimeout = setTimeout(this.stopRecording.bind(this), WebInspector.TimelineManager.DeadTimeRequiredToStopAutoRecordingEarly);
    },

    _mainResourceDidChange: function(event)
    {
        // Ignore resource events when there isn't a main frame yet. Those events are triggered by
        // loading the cached resources when the inspector opens, and they do not have timing information.
        if (!WebInspector.frameResourceManager.mainFrame)
            return;

        if (this._startAutoRecording(event))
            return;

        if (!this._recordingEnabled)
            return;

        var mainResource = event.target.mainResource;
        if (mainResource === this._autoRecordingMainResource)
            return;

        this._addRecord(new WebInspector.ResourceTimelineRecord(mainResource));
    },

    _resourceWasAdded: function(event)
    {
        // Ignore resource events when there isn't a main frame yet. Those events are triggered by
        // loading the cached resources when the inspector opens, and they do not have timing information.
        if (!WebInspector.frameResourceManager.mainFrame)
            return;

        if (!this._recordingEnabled)
            return;

        this._addRecord(new WebInspector.ResourceTimelineRecord(event.data.resource));
    }
};

WebInspector.TimelineManager.prototype.__proto__ = WebInspector.Object.prototype;
