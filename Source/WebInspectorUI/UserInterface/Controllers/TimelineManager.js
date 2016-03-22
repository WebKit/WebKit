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
        WebInspector.memoryManager.addEventListener(WebInspector.MemoryManager.Event.MemoryPressure, this._memoryPressure, this);

        this._enabledTimelineTypesSetting = new WebInspector.Setting("enabled-instrument-types", WebInspector.TimelineManager.defaultTimelineTypes());

        this._persistentNetworkTimeline = new WebInspector.NetworkTimeline;

        this._isCapturing = false;
        this._isCapturingPageReload = false;
        this._autoCapturingMainResource = null;
        this._boundStopCapturing = this.stopCapturing.bind(this);

        this._webTimelineScriptRecordsExpectingScriptProfilerEvents = null;
        this._scriptProfilerRecords = null;

        this.reset();
    }

    // Static

    static defaultTimelineTypes()
    {
        if (WebInspector.debuggableType === WebInspector.DebuggableType.JavaScript) {
            let defaultTypes = [WebInspector.TimelineRecord.Type.Script];
            if (WebInspector.HeapAllocationsInstrument.supported())
                defaultTypes.push(WebInspector.TimelineRecord.Type.HeapAllocations);
            return defaultTypes;
        }

        let defaultTypes = [
            WebInspector.TimelineRecord.Type.Network,
            WebInspector.TimelineRecord.Type.Layout,
            WebInspector.TimelineRecord.Type.Script,
        ];

        if (WebInspector.FPSInstrument.supported())
            defaultTypes.push(WebInspector.TimelineRecord.Type.RenderingFrame);

        return defaultTypes;
    }

    static availableTimelineTypes()
    {
        let types = WebInspector.TimelineManager.defaultTimelineTypes();
        if (WebInspector.debuggableType === WebInspector.DebuggableType.JavaScript)
            return types;

        if (WebInspector.MemoryInstrument.supported())
            types.push(WebInspector.TimelineRecord.Type.Memory);

        if (WebInspector.HeapAllocationsInstrument.supported())
            types.push(WebInspector.TimelineRecord.Type.HeapAllocations);

        return types;
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

    get enabledTimelineTypes()
    {
        let availableTimelineTypes = WebInspector.TimelineManager.availableTimelineTypes();
        return this._enabledTimelineTypesSetting.value.filter((type) => availableTimelineTypes.includes(type));
    }

    set enabledTimelineTypes(x)
    {
        this._enabledTimelineTypesSetting.value = x || [];
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
        // FIXME: <https://webkit.org/b/152904> Web Inspector: Timeline UI should keep up with processing all incoming records
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
        return this._scriptProfilerRecords !== null;
    }

    // Protected

    capturingStarted(startTime)
    {
        if (this._isCapturing)
            return;

        this._isCapturing = true;

        if (startTime)
            this.activeRecording.initializeTimeBoundsIfNecessary(startTime);

        this._webTimelineScriptRecordsExpectingScriptProfilerEvents = [];

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

    memoryTrackingStart(timestamp)
    {
        // Called from WebInspector.MemoryObserver.

        this.capturingStarted(timestamp);
    }

    memoryTrackingUpdate(event)
    {
        // Called from WebInspector.MemoryObserver.

        if (!this._isCapturing)
            return;

        this._addRecord(new WebInspector.MemoryTimelineRecord(event.timestamp, event.categories));
    }

    memoryTrackingComplete()
    {
        // Called from WebInspector.MemoryObserver.
    }

    heapTrackingStarted(timestamp, snapshot)
    {
        // Called from WebInspector.HeapObserver.

        this._addRecord(new WebInspector.HeapAllocationsTimelineRecord(timestamp, snapshot));

        this.capturingStarted(timestamp);
    }

    heapTrackingCompleted(timestamp, snapshot)
    {
        // Called from WebInspector.HeapObserver.

        this._addRecord(new WebInspector.HeapAllocationsTimelineRecord(timestamp, snapshot));
    }

    heapSnapshotAdded(timestamp, snapshot)
    {
        // Called from WebInspector.HeapAllocationsInstrument.

        this._addRecord(new WebInspector.HeapAllocationsTimelineRecord(timestamp, snapshot));
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

            var record;
            switch (parentRecordPayload && parentRecordPayload.type) {
            case TimelineAgent.EventType.TimerFire:
                record = new WebInspector.ScriptTimelineRecord(WebInspector.ScriptTimelineRecord.EventType.TimerFired, startTime, endTime, callFrames, sourceCodeLocation, parentRecordPayload.data.timerId, profileData);
                break;
            default:
                record = new WebInspector.ScriptTimelineRecord(WebInspector.ScriptTimelineRecord.EventType.ScriptEvaluated, startTime, endTime, callFrames, sourceCodeLocation, null, profileData);
                break;
            }

            this._webTimelineScriptRecordsExpectingScriptProfilerEvents.push(record);
            return record;

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
            if (!parentRecordPayload) {
                console.warn("Unexpectedly received a FunctionCall timeline record without a parent record");
                break;
            }

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

            var record;
            switch (parentRecordPayload.type) {
            case TimelineAgent.EventType.TimerFire:
                record = new WebInspector.ScriptTimelineRecord(WebInspector.ScriptTimelineRecord.EventType.TimerFired, startTime, endTime, callFrames, sourceCodeLocation, parentRecordPayload.data.timerId, profileData);
                break;
            case TimelineAgent.EventType.EventDispatch:
                record = new WebInspector.ScriptTimelineRecord(WebInspector.ScriptTimelineRecord.EventType.EventDispatched, startTime, endTime, callFrames, sourceCodeLocation, parentRecordPayload.data.type, profileData);
                break;
            case TimelineAgent.EventType.FireAnimationFrame:
                record = new WebInspector.ScriptTimelineRecord(WebInspector.ScriptTimelineRecord.EventType.AnimationFrameFired, startTime, endTime, callFrames, sourceCodeLocation, parentRecordPayload.data.id, profileData);
                break;
            case TimelineAgent.EventType.FunctionCall:
                record = new WebInspector.ScriptTimelineRecord(WebInspector.ScriptTimelineRecord.EventType.ScriptEvaluated, startTime, endTime, callFrames, sourceCodeLocation, parentRecordPayload.data.id, profileData);
                break;
            case TimelineAgent.EventType.RenderingFrame:
                record = new WebInspector.ScriptTimelineRecord(WebInspector.ScriptTimelineRecord.EventType.ScriptEvaluated, startTime, endTime, callFrames, sourceCodeLocation, parentRecordPayload.data.id, profileData);
                break;

            default:
                console.assert(false, "Missed FunctionCall embedded inside of: " + parentRecordPayload.type);
                break;
            }

            if (record) {
                this._webTimelineScriptRecordsExpectingScriptProfilerEvents.push(record);
                return record;
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

        let instruments = this.enabledTimelineTypes.map((type) => WebInspector.Instrument.createForTimelineType(type));
        let identifier = this._nextRecordingIdentifier++;
        let newRecording = new WebInspector.TimelineRecording(identifier, WebInspector.UIString("Timeline Recording %d").format(identifier), instruments);

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

    _memoryPressure(event)
    {
        if (!this._isCapturing)
            return;

        this.activeRecording.addMemoryPressureEvent(event.data.memoryPressureEvent);
    }

    _scriptProfilerTypeToScriptTimelineRecordType(type)
    {
        switch (type) {
        case ScriptProfilerAgent.EventType.API:
            return WebInspector.ScriptTimelineRecord.EventType.APIScriptEvaluated;
        case ScriptProfilerAgent.EventType.Microtask:
            return WebInspector.ScriptTimelineRecord.EventType.MicrotaskDispatched;
        case ScriptProfilerAgent.EventType.Other:
            return WebInspector.ScriptTimelineRecord.EventType.ScriptEvaluated;
        }
    }

    scriptProfilerTrackingStarted(timestamp)
    {
        this._scriptProfilerRecords = [];

        this.capturingStarted(timestamp);
    }

    scriptProfilerTrackingUpdated(event)
    {
        let {startTime, endTime, type} = event;
        let scriptRecordType = this._scriptProfilerTypeToScriptTimelineRecordType(type);
        let record = new WebInspector.ScriptTimelineRecord(scriptRecordType, startTime, endTime, null, null, null, null);
        record.__scriptProfilerType = type;
        this._scriptProfilerRecords.push(record);

        // "Other" events, generated by Web content, will have wrapping Timeline records
        // and need to be merged. Non-Other events, generated purely by the JavaScript
        // engine or outside of the page via APIs, will not have wrapping Timeline
        // records, so these records can just be added right now.
        if (type !== ScriptProfilerAgent.EventType.Other)
            this._addRecord(record);
    }

    scriptProfilerTrackingCompleted(samples)
    {
        console.assert(!this._webTimelineScriptRecordsExpectingScriptProfilerEvents || this._scriptProfilerRecords.length >= this._webTimelineScriptRecordsExpectingScriptProfilerEvents.length);

        if (samples) {
            // Associate the stackTraces with the ScriptProfiler created records.
            let {totalTime: totalExecutionTime, stackTraces} = samples;
            let topDownCallingContextTree = this.activeRecording.topDownCallingContextTree;
            let bottomUpCallingContextTree = this.activeRecording.bottomUpCallingContextTree;

            topDownCallingContextTree.increaseExecutionTime(totalExecutionTime);
            bottomUpCallingContextTree.increaseExecutionTime(totalExecutionTime);

            for (let i = 0; i < stackTraces.length; i++) {
                topDownCallingContextTree.updateTreeWithStackTrace(stackTraces[i]);
                bottomUpCallingContextTree.updateTreeWithStackTrace(stackTraces[i]);
            }

            // FIXME: This transformation should not be needed after introducing ProfileView.
            // Once we eliminate ProfileNodeTreeElements and ProfileNodeDataGridNodes.
            // <https://webkit.org/b/154973> Web Inspector: Timelines UI redesign: Remove TimelineSidebarPanel
            for (let i = 0; i < this._scriptProfilerRecords.length; ++i) {
                let record = this._scriptProfilerRecords[i];
                record.profilePayload = topDownCallingContextTree.toCPUProfilePayload(record.startTime, record.endTime);
            }
        }

        // Associate the ScriptProfiler created records with Web Timeline records.
        // Filter out the already added ScriptProfiler events which should not have been wrapped.
        if (WebInspector.debuggableType !== WebInspector.DebuggableType.JavaScript) {
            this._scriptProfilerRecords = this._scriptProfilerRecords.filter((x) => x.__scriptProfilerType === ScriptProfilerAgent.EventType.Other);
            this._mergeScriptProfileRecords();
        }

        this._scriptProfilerRecords = null;

        let timeline = this.activeRecording.timelineForRecordType(WebInspector.TimelineRecord.Type.Script);
        timeline.refresh();
    }

    _mergeScriptProfileRecords()
    {
        let nextRecord = function(list) { return list.shift() || null; }
        let nextWebTimelineRecord = nextRecord.bind(null, this._webTimelineScriptRecordsExpectingScriptProfilerEvents);
        let nextScriptProfilerRecord = nextRecord.bind(null, this._scriptProfilerRecords);
        let recordEnclosesRecord = function(record1, record2) {
            return record1.startTime <= record2.startTime && record1.endTime >= record2.endTime;
        }

        let webRecord = nextWebTimelineRecord();
        let profilerRecord = nextScriptProfilerRecord();

        while (webRecord && profilerRecord) {
            // Skip web records with parent web records. For example an EvaluateScript with an EvaluateScript parent.
            if (webRecord.parent instanceof WebInspector.ScriptTimelineRecord) {
                console.assert(recordEnclosesRecord(webRecord.parent, webRecord), "Timeline Record incorrectly wrapping another Timeline Record");
                webRecord = nextWebTimelineRecord();
                continue;
            }

            // Normal case of a Web record wrapping a Script record.
            if (recordEnclosesRecord(webRecord, profilerRecord)) {
                webRecord.profilePayload = profilerRecord.profilePayload;
                profilerRecord = nextScriptProfilerRecord();

                // If there are more script profile records in the same time interval, add them
                // as individual script evaluated records with profiles. This can happen with
                // web microtask checkpoints that are technically inside of other web records.
                // FIXME: <https://webkit.org/b/152903> Web Inspector: Timeline Cleanup: Better Timeline Record for Microtask Checkpoints
                while (profilerRecord && recordEnclosesRecord(webRecord, profilerRecord)) {
                    this._addRecord(profilerRecord);
                    profilerRecord = nextScriptProfilerRecord();
                }

                webRecord = nextWebTimelineRecord();
                continue;
            }

            // Profiler Record is entirely after the Web Record. This would mean an empty web record.
            if (profilerRecord.startTime > webRecord.endTime) {
                console.warn("Unexpected case of a Timeline record not containing a ScriptProfiler event and profile data");
                webRecord = nextWebTimelineRecord();
                continue;
            }

            // Non-wrapped profiler record.
            console.warn("Unexpected case of a ScriptProfiler event not being contained by a Timeline record");
            this._addRecord(profilerRecord);
            profilerRecord = nextScriptProfilerRecord();
        }

        // Skipping the remaining ScriptProfiler events to match the current UI for handling Timeline records.
        // However, the remaining ScriptProfiler records are valid and could be shown.
        // FIXME: <https://webkit.org/b/152904> Web Inspector: Timeline UI should keep up with processing all incoming records
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
