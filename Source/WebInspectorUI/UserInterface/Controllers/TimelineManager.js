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

WebInspector.TimelineManager = class TimelineManager extends WebInspector.Object
{
    constructor()
    {
        super();

        WebInspector.Frame.addEventListener(WebInspector.Frame.Event.ProvisionalLoadStarted, this._startAutoCapturing, this);
        WebInspector.Frame.addEventListener(WebInspector.Frame.Event.MainResourceDidChange, this._mainResourceDidChange, this);
        WebInspector.Frame.addEventListener(WebInspector.Frame.Event.ResourceWasAdded, this._resourceWasAdded, this);

        this._persistentNetworkTimeline = new WebInspector.NetworkTimeline;

        this._isCapturing = false;
        this._isCapturingPageReload = false;
        this._autoCapturingMainResource = null;
        this._boundStopCapturing = this.stopCapturing.bind(this);

        this.reset();
    }

    // Public

    reset()
    {
        if (this._isCapturing)
            this.stopCapturing();

        this._recordings = [];
        this._activeRecording = null;
        this._nextRecordingIdentifier = 1;

        this._loadNewRecording();
    }

    // The current recording that new timeline records will be appended to, if any.
    get activeRecording()
    {
        console.assert(this._activeRecording || !this._isCapturing);
        return this._activeRecording;
    }

    get persistentNetworkTimeline()
    {
        return this._persistentNetworkTimeline;
    }

    get recordings()
    {
        return this._recordings.slice();
    }

    get autoCaptureOnPageLoad()
    {
        return this._autoCaptureOnPageLoad;
    }

    set autoCaptureOnPageLoad(autoCapture)
    {
        this._autoCaptureOnPageLoad = autoCapture;
    }

    isCapturing()
    {
        return this._isCapturing;
    }

    isCapturingPageReload()
    {
        return this._isCapturingPageReload;
    }

    startCapturing(shouldCreateRecording)
    {
        console.assert(!this._isCapturing, "TimelineManager is already capturing.");

        if (!this._activeRecording || shouldCreateRecording)
            this._loadNewRecording();

        var result = TimelineAgent.start();

        // COMPATIBILITY (iOS 7): recordingStarted event did not exist yet. Start explicitly.
        if (!TimelineAgent.hasEvent("recordingStarted")) {
            result.then(function() {
                WebInspector.timelineManager.capturingStarted();
            });
        }
    }

    stopCapturing()
    {
        console.assert(this._isCapturing, "TimelineManager is not capturing.");

        TimelineAgent.stop();

        // NOTE: Always stop immediately instead of waiting for a Timeline.recordingStopped event.
        // This way the UI feels as responsive to a stop as possible.
        this.capturingStopped();
    }

    unloadRecording()
    {
        if (!this._activeRecording)
            return;

        if (this._isCapturing)
            this.stopCapturing();

        this._activeRecording.unloaded();
        this._activeRecording = null;
    }

    computeElapsedTime(timestamp)
    {
        if (!this._activeRecording)
            return 0;

        return this._activeRecording.computeElapsedTime(timestamp);
    }

    // Protected

    capturingStarted()
    {
        if (this._isCapturing)
            return;

        this._isCapturing = true;

        this.dispatchEventToListeners(WebInspector.TimelineManager.Event.CapturingStarted);
    }

    capturingStopped()
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
        this._isCapturingPageReload = false;
        this._autoCapturingMainResource = null;

        this.dispatchEventToListeners(WebInspector.TimelineManager.Event.CapturingStopped);
    }

    eventRecorded(recordPayload)
    {
        // Called from WebInspector.TimelineObserver.

        if (!this._isCapturing)
            return;

        var records = this._processNestedRecords(recordPayload);
        records.forEach(function(currentValue) {
            this._addRecord(currentValue);
        }.bind(this));
    }

    // Protected

    pageDidLoad(timestamp)
    {
        // Called from WebInspector.PageObserver.

        if (isNaN(WebInspector.frameResourceManager.mainFrame.loadEventTimestamp))
            WebInspector.frameResourceManager.mainFrame.markLoadEvent(this.activeRecording.computeElapsedTime(timestamp));
    }

    // Private

    _processNestedRecords(childRecordPayloads, parentRecordPayload)
    {
        // Convert to a single item array if needed.
        if (!(childRecordPayloads instanceof Array))
            childRecordPayloads = [childRecordPayloads];

        var records = [];

        // Iterate over the records tree using a stack. Doing this recursively has
        // been known to cause a call stack overflow. https://webkit.org/b/79106
        var stack = [{array: childRecordPayloads, parent: parentRecordPayload || null, index: 0}];
        while (stack.length) {
            var entry = stack.lastValue;
            var recordPayloads = entry.array;

            if (entry.index < recordPayloads.length) {
                var recordPayload = recordPayloads[entry.index];
                var record = this._processEvent(recordPayload, entry.parent);
                if (record)
                    records.push(record);

                if (recordPayload.children)
                    stack.push({array: recordPayload.children, parent: recordPayload, index: 0});
                ++entry.index;
            } else
                stack.pop();
        }

        return records;
    }

    _processRecord(recordPayload, parentRecordPayload)
    {
        var startTime = this.activeRecording.computeElapsedTime(recordPayload.startTime);
        var endTime = this.activeRecording.computeElapsedTime(recordPayload.endTime);
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
        case TimelineAgent.EventType.ScheduleStyleRecalculation:
            console.assert(isNaN(endTime));

            // Pass the startTime as the endTime since this record type has no duration.
            return new WebInspector.LayoutTimelineRecord(WebInspector.LayoutTimelineRecord.EventType.InvalidateStyles, startTime, startTime, callFrames, sourceCodeLocation);

        case TimelineAgent.EventType.RecalculateStyles:
            return new WebInspector.LayoutTimelineRecord(WebInspector.LayoutTimelineRecord.EventType.RecalculateStyles, startTime, endTime, callFrames, sourceCodeLocation);

        case TimelineAgent.EventType.InvalidateLayout:
            console.assert(isNaN(endTime));

            // Pass the startTime as the endTime since this record type has no duration.
            return new WebInspector.LayoutTimelineRecord(WebInspector.LayoutTimelineRecord.EventType.InvalidateLayout, startTime, startTime, callFrames, sourceCodeLocation);

        case TimelineAgent.EventType.Layout:
            var layoutRecordType = sourceCodeLocation ? WebInspector.LayoutTimelineRecord.EventType.ForcedLayout : WebInspector.LayoutTimelineRecord.EventType.Layout;

            // COMPATIBILITY (iOS 6): Layout records did not contain area properties. This is not exposed via a quad "root".
            var quad = recordPayload.data.root ? new WebInspector.Quad(recordPayload.data.root) : null;
            if (quad)
                return new WebInspector.LayoutTimelineRecord(layoutRecordType, startTime, endTime, callFrames, sourceCodeLocation, quad.points[0].x, quad.points[0].y, quad.width, quad.height, quad);
            else
                return new WebInspector.LayoutTimelineRecord(layoutRecordType, startTime, endTime, callFrames, sourceCodeLocation);

        case TimelineAgent.EventType.Paint:
            // COMPATIBILITY (iOS 6): Paint records data contained x, y, width, height properties. This became a quad "clip".
            var quad = recordPayload.data.clip ? new WebInspector.Quad(recordPayload.data.clip) : null;
            var duringComposite = recordPayload.__duringComposite || false;
            if (quad)
                return new WebInspector.LayoutTimelineRecord(WebInspector.LayoutTimelineRecord.EventType.Paint, startTime, endTime, callFrames, sourceCodeLocation, null, null, quad.width, quad.height, quad, duringComposite);
            else
                return new WebInspector.LayoutTimelineRecord(WebInspector.LayoutTimelineRecord.EventType.Paint, startTime, endTime, callFrames, sourceCodeLocation, recordPayload.data.x, recordPayload.data.y, recordPayload.data.width, recordPayload.data.height, null, duringComposite);

        case TimelineAgent.EventType.Composite:
            recordPayload.children.forEach(function(childRecordPayload) {
                console.assert(childRecordPayload.type === TimelineAgent.EventType.Paint, childRecordPayload.type);
                childRecordPayload.__duringComposite = true;
            });
            return new WebInspector.LayoutTimelineRecord(WebInspector.LayoutTimelineRecord.EventType.Composite, startTime, endTime, callFrames, sourceCodeLocation);

        case TimelineAgent.EventType.RenderingFrame:
            if (!recordPayload.children)
                return null;

            var children = this._processNestedRecords(recordPayload.children, recordPayload);
            if (!children.length)
                return null;
            return new WebInspector.RenderingFrameTimelineRecord(startTime, endTime, children);

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
                return new WebInspector.ScriptTimelineRecord(WebInspector.ScriptTimelineRecord.EventType.TimerFired, startTime, endTime, callFrames, sourceCodeLocation, parentRecordPayload.data.timerId, profileData);
            default:
                return new WebInspector.ScriptTimelineRecord(WebInspector.ScriptTimelineRecord.EventType.ScriptEvaluated, startTime, endTime, callFrames, sourceCodeLocation, null, profileData);
            }

            break;

        case TimelineAgent.EventType.ConsoleProfile:
            var profileData = recordPayload.data.profile;
            console.assert(profileData);
            return new WebInspector.ScriptTimelineRecord(WebInspector.ScriptTimelineRecord.EventType.ConsoleProfileRecorded, startTime, endTime, callFrames, sourceCodeLocation, recordPayload.data.title, profileData);

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
                return new WebInspector.ScriptTimelineRecord(WebInspector.ScriptTimelineRecord.EventType.TimerFired, startTime, endTime, callFrames, sourceCodeLocation, parentRecordPayload.data.timerId, profileData);
            case TimelineAgent.EventType.EventDispatch:
                return new WebInspector.ScriptTimelineRecord(WebInspector.ScriptTimelineRecord.EventType.EventDispatched, startTime, endTime, callFrames, sourceCodeLocation, parentRecordPayload.data.type, profileData);
            case TimelineAgent.EventType.XHRLoad:
                return new WebInspector.ScriptTimelineRecord(WebInspector.ScriptTimelineRecord.EventType.EventDispatched, startTime, endTime, callFrames, sourceCodeLocation, "load", profileData);
            case TimelineAgent.EventType.XHRReadyStateChange:
                return new WebInspector.ScriptTimelineRecord(WebInspector.ScriptTimelineRecord.EventType.EventDispatched, startTime, endTime, callFrames, sourceCodeLocation, "readystatechange", profileData);
            case TimelineAgent.EventType.FireAnimationFrame:
                return new WebInspector.ScriptTimelineRecord(WebInspector.ScriptTimelineRecord.EventType.AnimationFrameFired, startTime, endTime, callFrames, sourceCodeLocation, parentRecordPayload.data.id, profileData);
            }

            break;

        case TimelineAgent.EventType.ProbeSample:
            // Pass the startTime as the endTime since this record type has no duration.
            sourceCodeLocation = WebInspector.probeManager.probeForIdentifier(recordPayload.data.probeId).breakpoint.sourceCodeLocation;
            return new WebInspector.ScriptTimelineRecord(WebInspector.ScriptTimelineRecord.EventType.ProbeSampleRecorded, startTime, startTime, callFrames, sourceCodeLocation, recordPayload.data.probeId);

        case TimelineAgent.EventType.TimerInstall:
            console.assert(isNaN(endTime));

            // Pass the startTime as the endTime since this record type has no duration.
            return new WebInspector.ScriptTimelineRecord(WebInspector.ScriptTimelineRecord.EventType.TimerInstalled, startTime, startTime, callFrames, sourceCodeLocation, recordPayload.data.timerId);

        case TimelineAgent.EventType.TimerRemove:
            console.assert(isNaN(endTime));

            // Pass the startTime as the endTime since this record type has no duration.
            return new WebInspector.ScriptTimelineRecord(WebInspector.ScriptTimelineRecord.EventType.TimerRemoved, startTime, startTime, callFrames, sourceCodeLocation, recordPayload.data.timerId);

        case TimelineAgent.EventType.RequestAnimationFrame:
            console.assert(isNaN(endTime));

            // Pass the startTime as the endTime since this record type has no duration.
            return new WebInspector.ScriptTimelineRecord(WebInspector.ScriptTimelineRecord.EventType.AnimationFrameRequested, startTime, startTime, callFrames, sourceCodeLocation, recordPayload.data.timerId);

        case TimelineAgent.EventType.CancelAnimationFrame:
            console.assert(isNaN(endTime));

            // Pass the startTime as the endTime since this record type has no duration.
            return new WebInspector.ScriptTimelineRecord(WebInspector.ScriptTimelineRecord.EventType.AnimationFrameCanceled, startTime, startTime, callFrames, sourceCodeLocation, recordPayload.data.timerId);
        }

        return null;
    }

    _processEvent(recordPayload, parentRecordPayload)
    {
        var startTime = this.activeRecording.computeElapsedTime(recordPayload.startTime);
        var endTime = this.activeRecording.computeElapsedTime(recordPayload.endTime);

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

        case TimelineAgent.EventType.TimeStamp:
            var eventMarker = new WebInspector.TimelineMarker(startTime, WebInspector.TimelineMarker.Type.TimeStamp);
            this._activeRecording.addEventMarker(eventMarker);
            break;

        default:
            return this._processRecord(recordPayload, parentRecordPayload);
        }

        return null;
    }

    _loadNewRecording()
    {
        if (this._activeRecording && this._activeRecording.isEmpty())
            return;

        var identifier = this._nextRecordingIdentifier++;
        var newRecording = new WebInspector.TimelineRecording(identifier, WebInspector.UIString("Timeline Recording %d").format(identifier));
        newRecording.addTimeline(WebInspector.Timeline.create(WebInspector.TimelineRecord.Type.Network, newRecording));
        newRecording.addTimeline(WebInspector.Timeline.create(WebInspector.TimelineRecord.Type.Layout, newRecording));
        newRecording.addTimeline(WebInspector.Timeline.create(WebInspector.TimelineRecord.Type.Script, newRecording));

        // COMPATIBILITY (iOS 8): TimelineAgent.EventType.RenderingFrame did not exist.
        if (window.TimelineAgent && TimelineAgent.EventType.RenderingFrame)
            newRecording.addTimeline(WebInspector.Timeline.create(WebInspector.TimelineRecord.Type.RenderingFrame, newRecording));

        this._recordings.push(newRecording);
        this.dispatchEventToListeners(WebInspector.TimelineManager.Event.RecordingCreated, {recording: newRecording});

        console.assert(newRecording.isWritable());

        if (this._isCapturing)
            this.stopCapturing();

        var oldRecording = this._activeRecording;
        if (oldRecording)
            oldRecording.unloaded();

        this._activeRecording = newRecording;

        // COMPATIBILITY (iOS 8): When using Legacy timestamps, a navigation will have computed
        // the main resource's will send request timestamp in terms of the last page's base timestamp.
        // Now that we have navigated, we should reset the legacy base timestamp and the
        // will send request timestamp for the new main resource. This way, all new timeline
        // records will be computed relative to the new navigation.
        if (this._autoCapturingMainResource && WebInspector.TimelineRecording.isLegacy) {
            console.assert(this._autoCapturingMainResource.originalRequestWillBeSentTimestamp);
            this._activeRecording.setLegacyBaseTimestamp(this._autoCapturingMainResource.originalRequestWillBeSentTimestamp);
            this._autoCapturingMainResource._requestSentTimestamp = 0;
        }

        this.dispatchEventToListeners(WebInspector.TimelineManager.Event.RecordingLoaded, {oldRecording});
    }

    _callFramesFromPayload(payload)
    {
        if (!payload)
            return null;

        return payload.map(WebInspector.CallFrame.fromPayload);
    }

    _addRecord(record)
    {
        this._activeRecording.addRecord(record);

        // Only worry about dead time after the load event.
        if (!isNaN(WebInspector.frameResourceManager.mainFrame.loadEventTimestamp))
            this._resetAutoRecordingDeadTimeTimeout();
    }

    _startAutoCapturing(event)
    {
        if (!this._autoCaptureOnPageLoad)
            return false;

        if (!event.target.isMainFrame() || (this._isCapturing && !this._autoCapturingMainResource))
            return false;

        var mainResource = event.target.provisionalMainResource || event.target.mainResource;
        if (mainResource === this._autoCapturingMainResource)
            return false;

        var oldMainResource = event.target.mainResource || null;
        this._isCapturingPageReload = oldMainResource !== null && oldMainResource.url === mainResource.url;

        if (this._isCapturing)
            this.stopCapturing();

        this._autoCapturingMainResource = mainResource;

        this._loadNewRecording();

        this.startCapturing();

        this._addRecord(new WebInspector.ResourceTimelineRecord(mainResource));

        if (this._stopCapturingTimeout)
            clearTimeout(this._stopCapturingTimeout);
        this._stopCapturingTimeout = setTimeout(this._boundStopCapturing, WebInspector.TimelineManager.MaximumAutoRecordDuration);

        return true;
    }

    _stopAutoRecordingSoon()
    {
        // Only auto stop when auto capturing.
        if (!this._isCapturing || !this._autoCapturingMainResource)
            return;

        if (this._stopCapturingTimeout)
            clearTimeout(this._stopCapturingTimeout);
        this._stopCapturingTimeout = setTimeout(this._boundStopCapturing, WebInspector.TimelineManager.MaximumAutoRecordDurationAfterLoadEvent);
    }

    _resetAutoRecordingDeadTimeTimeout()
    {
        // Only monitor dead time when auto capturing.
        if (!this._isCapturing || !this._autoCapturingMainResource)
            return;

        if (this._deadTimeTimeout)
            clearTimeout(this._deadTimeTimeout);
        this._deadTimeTimeout = setTimeout(this._boundStopCapturing, WebInspector.TimelineManager.DeadTimeRequiredToStopAutoRecordingEarly);
    }

    _mainResourceDidChange(event)
    {
        if (event.target.isMainFrame())
            this._persistentNetworkTimeline.reset();

        var mainResource = event.target.mainResource;
        var record = new WebInspector.ResourceTimelineRecord(mainResource);
        if (!isNaN(record.startTime))
            this._persistentNetworkTimeline.addRecord(record);

        // Ignore resource events when there isn't a main frame yet. Those events are triggered by
        // loading the cached resources when the inspector opens, and they do not have timing information.
        if (!WebInspector.frameResourceManager.mainFrame)
            return;

        if (this._startAutoCapturing(event))
            return;

        if (!this._isCapturing)
            return;

        if (mainResource === this._autoCapturingMainResource)
            return;

        this._addRecord(record);
    }

    _resourceWasAdded(event)
    {
        var record = new WebInspector.ResourceTimelineRecord(event.data.resource);
        if (!isNaN(record.startTime))
            this._persistentNetworkTimeline.addRecord(record);

        // Ignore resource events when there isn't a main frame yet. Those events are triggered by
        // loading the cached resources when the inspector opens, and they do not have timing information.
        if (!WebInspector.frameResourceManager.mainFrame)
            return;

        if (!this._isCapturing)
            return;

        this._addRecord(record);
    }
};

WebInspector.TimelineManager.Event = {
    RecordingCreated: "timeline-manager-recording-created",
    RecordingLoaded: "timeline-manager-recording-loaded",
    CapturingStarted: "timeline-manager-capturing-started",
    CapturingStopped: "timeline-manager-capturing-stopped"
};

WebInspector.TimelineManager.MaximumAutoRecordDuration = 90000; // 90 seconds
WebInspector.TimelineManager.MaximumAutoRecordDurationAfterLoadEvent = 10000; // 10 seconds
WebInspector.TimelineManager.DeadTimeRequiredToStopAutoRecordingEarly = 2000; // 2 seconds
