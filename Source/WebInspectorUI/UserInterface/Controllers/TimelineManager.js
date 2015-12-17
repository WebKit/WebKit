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

        WebInspector.heapManager.addEventListener(WebInspector.HeapManager.Event.GarbageCollected, this._garbageCollected, this);

        this._persistentNetworkTimeline = new WebInspector.NetworkTimeline;

        this._isCapturing = false;
        this._isCapturingPageReload = false;
        this._autoCapturingMainResource = null;
        this._boundStopCapturing = this.stopCapturing.bind(this);

        this._scriptProfileRecords = null;

        this.reset();
    }

    // Static

    static defaultInstruments()
    {
        if (WebInspector.debuggableType === WebInspector.DebuggableType.JavaScript)
            return [new WebInspector.ScriptInstrument];

        let defaults = [
            new WebInspector.NetworkInstrument,
            new WebInspector.LayoutInstrument,
            new WebInspector.ScriptInstrument,
        ];

        if (WebInspector.FPSInstrument.supported())
            defaults.push(new WebInspector.FPSInstrument);

        return defaults;
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

        this._activeRecording.start();
    }

    stopCapturing()
    {
        console.assert(this._isCapturing, "TimelineManager is not capturing.");

        this._activeRecording.stop();

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

    scriptProfilerIsTracking()
    {
        return this._scriptProfileRecords !== null;
    }

    // Protected

    capturingStarted(startTime)
    {
        if (this._isCapturing)
            return;

        this._isCapturing = true;

        if (startTime)
            this.activeRecording.initializeTimeBoundsIfNecessary(startTime);

        this.dispatchEventToListeners(WebInspector.TimelineManager.Event.CapturingStarted, {startTime});
    }

    capturingStopped(endTime)
    {
        if (!this._isCapturing)
            return;

        if (this._stopCapturingTimeout) {
            clearTimeout(this._stopCapturingTimeout);
            this._stopCapturingTimeout = undefined;
        }

        if (this._deadTimeTimeout) {
            clearTimeout(this._deadTimeTimeout);
            this._deadTimeTimeout = undefined;
        }

        this._isCapturing = false;
        this._isCapturingPageReload = false;
        this._autoCapturingMainResource = null;

        this.dispatchEventToListeners(WebInspector.TimelineManager.Event.CapturingStopped, {endTime});
    }

    eventRecorded(recordPayload)
    {
        // Called from WebInspector.TimelineObserver.

        if (!this._isCapturing)
            return;

        var records = [];

        // Iterate over the records tree using a stack. Doing this recursively has
        // been known to cause a call stack overflow. https://webkit.org/b/79106
        var stack = [{array: [recordPayload], parent: null, parentRecord: null, index: 0}];
        while (stack.length) {
            var entry = stack.lastValue;
            var recordPayloads = entry.array;

            if (entry.index < recordPayloads.length) {
                var recordPayload = recordPayloads[entry.index];
                var record = this._processEvent(recordPayload, entry.parent);
                if (record) {
                    record.parent = entry.parentRecord;
                    records.push(record);
                    if (entry.parentRecord)
                        entry.parentRecord.children.push(record);
                }

                if (recordPayload.children && recordPayload.children.length)
                    stack.push({array: recordPayload.children, parent: recordPayload, parentRecord: record || entry.parentRecord, index: 0});
                ++entry.index;
            } else
                stack.pop();
        }

        for (var record of records) {
            if (record.type === WebInspector.TimelineRecord.Type.RenderingFrame) {
                if (!record.children.length)
                    continue;
                record.setupFrameIndex();
            }

            this._addRecord(record);
        }
    }

    // Protected

    pageDOMContentLoadedEventFired(timestamp)
    {
        // Called from WebInspector.PageObserver.

        console.assert(this._activeRecording);
        console.assert(isNaN(WebInspector.frameResourceManager.mainFrame.domContentReadyEventTimestamp));

        let computedTimestamp = this.activeRecording.computeElapsedTime(timestamp);

        WebInspector.frameResourceManager.mainFrame.markDOMContentReadyEvent(computedTimestamp);

        let eventMarker = new WebInspector.TimelineMarker(computedTimestamp, WebInspector.TimelineMarker.Type.DOMContentEvent);
        this._activeRecording.addEventMarker(eventMarker);
    }

    pageLoadEventFired(timestamp)
    {
        // Called from WebInspector.PageObserver.

        console.assert(this._activeRecording);
        console.assert(isNaN(WebInspector.frameResourceManager.mainFrame.loadEventTimestamp));

        let computedTimestamp = this.activeRecording.computeElapsedTime(timestamp);

        WebInspector.frameResourceManager.mainFrame.markLoadEvent(computedTimestamp);

        let eventMarker = new WebInspector.TimelineMarker(computedTimestamp, WebInspector.TimelineMarker.Type.LoadEvent);
        this._activeRecording.addEventMarker(eventMarker);

        this._stopAutoRecordingSoon();
    }

    // Private

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
            var quad = new WebInspector.Quad(recordPayload.data.root);
            return new WebInspector.LayoutTimelineRecord(layoutRecordType, startTime, endTime, callFrames, sourceCodeLocation, quad);

        case TimelineAgent.EventType.Paint:
            var quad = new WebInspector.Quad(recordPayload.data.clip);
            return new WebInspector.LayoutTimelineRecord(WebInspector.LayoutTimelineRecord.EventType.Paint, startTime, endTime, callFrames, sourceCodeLocation, quad);

        case TimelineAgent.EventType.Composite:
            return new WebInspector.LayoutTimelineRecord(WebInspector.LayoutTimelineRecord.EventType.Composite, startTime, endTime, callFrames, sourceCodeLocation);

        case TimelineAgent.EventType.RenderingFrame:
            if (!recordPayload.children || !recordPayload.children.length)
                return null;

            return new WebInspector.RenderingFrameTimelineRecord(startTime, endTime);

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

        case TimelineAgent.EventType.TimerFire:
        case TimelineAgent.EventType.EventDispatch:
        case TimelineAgent.EventType.FireAnimationFrame:
            // These are handled when the parent of FunctionCall or EvaluateScript.
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
                return new WebInspector.ScriptTimelineRecord(WebInspector.ScriptTimelineRecord.EventType.TimerFired, startTime, endTime, callFrames, sourceCodeLocation, parentRecordPayload.data.timerId, profileData);
            case TimelineAgent.EventType.EventDispatch:
                return new WebInspector.ScriptTimelineRecord(WebInspector.ScriptTimelineRecord.EventType.EventDispatched, startTime, endTime, callFrames, sourceCodeLocation, parentRecordPayload.data.type, profileData);
            case TimelineAgent.EventType.FireAnimationFrame:
                return new WebInspector.ScriptTimelineRecord(WebInspector.ScriptTimelineRecord.EventType.AnimationFrameFired, startTime, endTime, callFrames, sourceCodeLocation, parentRecordPayload.data.id, profileData);
            default:
                console.assert(false, "Missed FunctionCall embedded inside of: " + parentRecordPayload.type);
            }

            break;

        case TimelineAgent.EventType.ProbeSample:
            // Pass the startTime as the endTime since this record type has no duration.
            sourceCodeLocation = WebInspector.probeManager.probeForIdentifier(recordPayload.data.probeId).breakpoint.sourceCodeLocation;
            return new WebInspector.ScriptTimelineRecord(WebInspector.ScriptTimelineRecord.EventType.ProbeSampleRecorded, startTime, startTime, callFrames, sourceCodeLocation, recordPayload.data.probeId);

        case TimelineAgent.EventType.TimerInstall:
            console.assert(isNaN(endTime));

            // Pass the startTime as the endTime since this record type has no duration.
            let timerDetails = {timerId: recordPayload.data.timerId, timeout: recordPayload.data.timeout, repeating: !recordPayload.data.singleShot};
            return new WebInspector.ScriptTimelineRecord(WebInspector.ScriptTimelineRecord.EventType.TimerInstalled, startTime, startTime, callFrames, sourceCodeLocation, timerDetails);

        case TimelineAgent.EventType.TimerRemove:
            console.assert(isNaN(endTime));

            // Pass the startTime as the endTime since this record type has no duration.
            return new WebInspector.ScriptTimelineRecord(WebInspector.ScriptTimelineRecord.EventType.TimerRemoved, startTime, startTime, callFrames, sourceCodeLocation, recordPayload.data.timerId);

        case TimelineAgent.EventType.RequestAnimationFrame:
            console.assert(isNaN(endTime));

            // Pass the startTime as the endTime since this record type has no duration.
            return new WebInspector.ScriptTimelineRecord(WebInspector.ScriptTimelineRecord.EventType.AnimationFrameRequested, startTime, startTime, callFrames, sourceCodeLocation, recordPayload.data.id);

        case TimelineAgent.EventType.CancelAnimationFrame:
            console.assert(isNaN(endTime));

            // Pass the startTime as the endTime since this record type has no duration.
            return new WebInspector.ScriptTimelineRecord(WebInspector.ScriptTimelineRecord.EventType.AnimationFrameCanceled, startTime, startTime, callFrames, sourceCodeLocation, recordPayload.data.id);

        default:
            console.error("Missing handling of Timeline Event Type: " + recordPayload.type);
        }

        return null;
    }

    _processEvent(recordPayload, parentRecordPayload)
    {
        switch (recordPayload.type) {
        case TimelineAgent.EventType.TimeStamp:
            var timestamp = this.activeRecording.computeElapsedTime(recordPayload.startTime);
            var eventMarker = new WebInspector.TimelineMarker(timestamp, WebInspector.TimelineMarker.Type.TimeStamp, recordPayload.data.message);
            this._activeRecording.addEventMarker(eventMarker);
            break;

        case TimelineAgent.EventType.Time:
        case TimelineAgent.EventType.TimeEnd:
            // FIXME: <https://webkit.org/b/150690> Web Inspector: Show console.time/timeEnd ranges in Timeline
            // FIXME: Make use of "message" payload properties.
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

        // FIXME: Move the list of instruments for a new recording to a Setting when new Instruments are supported.
        let instruments = WebInspector.TimelineManager.defaultInstruments();

        var identifier = this._nextRecordingIdentifier++;
        var newRecording = new WebInspector.TimelineRecording(identifier, WebInspector.UIString("Timeline Recording %d").format(identifier), instruments);

        this._recordings.push(newRecording);
        this.dispatchEventToListeners(WebInspector.TimelineManager.Event.RecordingCreated, {recording: newRecording});

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
        if (WebInspector.frameResourceManager.mainFrame && isNaN(WebInspector.frameResourceManager.mainFrame.loadEventTimestamp))
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

    _garbageCollected(event)
    {
        if (!this._isCapturing)
            return;

        let collection = event.data.collection;
        this._addRecord(new WebInspector.ScriptTimelineRecord(WebInspector.ScriptTimelineRecord.EventType.GarbageCollected, collection.startTime, collection.endTime, null, null, collection));
    }

    scriptProfilerTrackingStarted(timestamp)
    {
        this._scriptProfileRecords = [];

        this.capturingStarted(timestamp);
    }

    scriptProfilerTrackingUpdated(event)
    {
        let {startTime, endTime, type} = event;
        let scriptRecordType = type === ScriptProfilerAgent.EventType.Microtask ? WebInspector.ScriptTimelineRecord.EventType.MicrotaskDispatched : WebInspector.ScriptTimelineRecord.EventType.ScriptEvaluated;
        let record = new WebInspector.ScriptTimelineRecord(scriptRecordType, startTime, endTime, null, null, null, null);
        this._scriptProfileRecords.push(record);
        this._addRecord(record);
    }

    scriptProfilerTrackingCompleted(profiles)
    {
        if (profiles) {
            console.assert(this._scriptProfileRecords.length === profiles.length, this._scriptProfileRecords.length, profiles.length);
            for (let i = 0; i < this._scriptProfileRecords.length; ++i)
                this._scriptProfileRecords[i].setProfilePayload(profiles[i]);
        }

        this._scriptProfileRecords = null;

        let timeline = this.activeRecording.timelineForRecordType(WebInspector.TimelineRecord.Type.Script);
        timeline.refresh();
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
