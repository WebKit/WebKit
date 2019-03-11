/*
 * Copyright (C) 2013, 2016 Apple Inc. All rights reserved.
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

// FIXME: TimelineManager lacks advanced multi-target support. (Instruments/Profilers per-target)

WI.TimelineManager = class TimelineManager extends WI.Object
{
    constructor()
    {
        super();

        WI.Frame.addEventListener(WI.Frame.Event.ProvisionalLoadStarted, this._provisionalLoadStarted, this);
        WI.Frame.addEventListener(WI.Frame.Event.MainResourceDidChange, this._mainResourceDidChange, this);
        WI.Frame.addEventListener(WI.Frame.Event.ResourceWasAdded, this._resourceWasAdded, this);
        WI.Target.addEventListener(WI.Target.Event.ResourceAdded, this._resourceWasAdded, this);

        WI.heapManager.addEventListener(WI.HeapManager.Event.GarbageCollected, this._garbageCollected, this);
        WI.memoryManager.addEventListener(WI.MemoryManager.Event.MemoryPressure, this._memoryPressure, this);

        WI.settings.timelinesAutoStop.addEventListener(WI.Setting.Event.Changed, this._handleTimelinesAutoStopSettingChanged, this);

        this._enabledTimelineTypesSetting = new WI.Setting("enabled-instrument-types", WI.TimelineManager.defaultTimelineTypes());

        this._isCapturing = false;
        this._initiatedByBackendStart = false;
        this._initiatedByBackendStop = false;
        this._waitingForCapturingStartedEvent = false;
        this._isCapturingPageReload = false;
        this._autoCaptureOnPageLoad = false;
        this._mainResourceForAutoCapturing = null;
        this._shouldSetAutoCapturingMainResource = false;
        this._transitioningPageTarget = false;
        this._boundStopCapturing = this.stopCapturing.bind(this);

        this._webTimelineScriptRecordsExpectingScriptProfilerEvents = null;
        this._scriptProfilerRecords = null;

        this._stopCapturingTimeout = undefined;
        this._deadTimeTimeout = undefined;
        this._lastDeadTimeTickle = 0;

        this.reset();
    }

    // Target

    initializeTarget(target)
    {
        if (target.TimelineAgent) {
            this._updateAutoCaptureInstruments([target]);

            // COMPATIBILITY (iOS 9): Timeline.setAutoCaptureEnabled did not exist.
            if (target.TimelineAgent.setAutoCaptureEnabled)
                target.TimelineAgent.setAutoCaptureEnabled(this._autoCaptureOnPageLoad);
        }
    }

    transitionPageTarget()
    {
        this._transitioningPageTarget = true;
    }

    // Static

    static defaultTimelineTypes()
    {
        if (WI.sharedApp.debuggableType === WI.DebuggableType.JavaScript) {
            let defaultTypes = [WI.TimelineRecord.Type.Script];
            if (WI.HeapAllocationsInstrument.supported())
                defaultTypes.push(WI.TimelineRecord.Type.HeapAllocations);
            return defaultTypes;
        }

        if (WI.sharedApp.debuggableType === WI.DebuggableType.ServiceWorker) {
            // FIXME: Support Network Timeline in ServiceWorker.
            let defaultTypes = [WI.TimelineRecord.Type.Script];
            if (WI.HeapAllocationsInstrument.supported())
                defaultTypes.push(WI.TimelineRecord.Type.HeapAllocations);
            return defaultTypes;
        }

        let defaultTypes = [
            WI.TimelineRecord.Type.Network,
            WI.TimelineRecord.Type.Layout,
            WI.TimelineRecord.Type.Script,
        ];

        if (WI.CPUInstrument.supported())
            defaultTypes.push(WI.TimelineRecord.Type.CPU);

        if (WI.FPSInstrument.supported())
            defaultTypes.push(WI.TimelineRecord.Type.RenderingFrame);

        return defaultTypes;
    }

    static availableTimelineTypes()
    {
        let types = WI.TimelineManager.defaultTimelineTypes();
        if (WI.sharedApp.debuggableType === WI.DebuggableType.JavaScript || WI.sharedApp.debuggableType === WI.DebuggableType.ServiceWorker)
            return types;

        if (WI.MemoryInstrument.supported())
            types.push(WI.TimelineRecord.Type.Memory);

        if (WI.HeapAllocationsInstrument.supported())
            types.push(WI.TimelineRecord.Type.HeapAllocations);

        if (WI.MediaInstrument.supported()) {
            let insertionIndex = types.indexOf(WI.TimelineRecord.Type.Layout) + 1;
            types.insertAtIndex(WI.TimelineRecord.Type.Media, insertionIndex || types.length);
        }

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
        autoCapture = !!autoCapture;

        if (this._autoCaptureOnPageLoad === autoCapture)
            return;

        this._autoCaptureOnPageLoad = autoCapture;

        if (window.TimelineAgent && TimelineAgent.setAutoCaptureEnabled)
            TimelineAgent.setAutoCaptureEnabled(this._autoCaptureOnPageLoad);
    }

    get enabledTimelineTypes()
    {
        let availableTimelineTypes = WI.TimelineManager.availableTimelineTypes();
        return this._enabledTimelineTypesSetting.value.filter((type) => availableTimelineTypes.includes(type));
    }

    set enabledTimelineTypes(x)
    {
        this._enabledTimelineTypesSetting.value = x || [];

        this._updateAutoCaptureInstruments(WI.targets);
    }

    isCapturing()
    {
        return this._isCapturing;
    }

    isCapturingPageReload()
    {
        return this._isCapturingPageReload;
    }

    willAutoStop()
    {
        return !!this._stopCapturingTimeout;
    }

    relaxAutoStop()
    {
        if (this._stopCapturingTimeout) {
            clearTimeout(this._stopCapturingTimeout);
            this._stopCapturingTimeout = undefined;
        }

        if (this._deadTimeTimeout) {
            clearTimeout(this._deadTimeTimeout);
            this._deadTimeTimeout = undefined;
        }
    }

    startCapturing(shouldCreateRecording)
    {
        console.assert(!this._isCapturing, "TimelineManager is already capturing.");

        if (!this._activeRecording || shouldCreateRecording)
            this._loadNewRecording();

        this._waitingForCapturingStartedEvent = true;

        this.dispatchEventToListeners(WI.TimelineManager.Event.CapturingWillStart);

        this._activeRecording.start(this._initiatedByBackendStart);
    }

    stopCapturing()
    {
        console.assert(this._isCapturing, "TimelineManager is not capturing.");

        this._activeRecording.stop(this._initiatedByBackendStop);

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
        // Called from WI.TimelineObserver.

        if (this._isCapturing)
            return;

        this._waitingForCapturingStartedEvent = false;
        this._isCapturing = true;

        this._lastDeadTimeTickle = 0;

        if (startTime)
            this.activeRecording.initializeTimeBoundsIfNecessary(startTime);

        this._webTimelineScriptRecordsExpectingScriptProfilerEvents = [];

        WI.DOMNode.addEventListener(WI.DOMNode.Event.DidFireEvent, this._handleDOMNodeDidFireEvent, this);
        WI.DOMNode.addEventListener(WI.DOMNode.Event.LowPowerChanged, this._handleDOMNodeLowPowerChanged, this);

        this.dispatchEventToListeners(WI.TimelineManager.Event.CapturingStarted, {startTime});
    }

    capturingStopped(endTime)
    {
        // Called from WI.TimelineObserver.

        if (!this._isCapturing)
            return;

        WI.DOMNode.removeEventListener(null, null, this);

        this.relaxAutoStop();

        this._isCapturing = false;
        this._isCapturingPageReload = false;
        this._shouldSetAutoCapturingMainResource = false;
        this._mainResourceForAutoCapturing = null;
        this._initiatedByBackendStart = false;
        this._initiatedByBackendStop = false;

        this.dispatchEventToListeners(WI.TimelineManager.Event.CapturingStopped, {endTime});
    }

    autoCaptureStarted()
    {
        // Called from WI.TimelineObserver.

        if (this._isCapturing)
            this.stopCapturing();

        this._initiatedByBackendStart = true;

        // We may already have an fresh TimelineRecording created if autoCaptureStarted is received
        // between sending the Timeline.start command and receiving Timeline.capturingStarted event.
        // In that case, there is no need to call startCapturing again. Reuse the fresh recording.
        if (!this._waitingForCapturingStartedEvent) {
            const createNewRecording = true;
            this.startCapturing(createNewRecording);
        }

        this._shouldSetAutoCapturingMainResource = true;
    }

    programmaticCaptureStarted()
    {
        // Called from WI.TimelineObserver.

        this._initiatedByBackendStart = true;

        this._activeRecording.addScriptInstrumentForProgrammaticCapture();

        const createNewRecording = false;
        this.startCapturing(createNewRecording);
    }

    programmaticCaptureStopped()
    {
        // Called from WI.TimelineObserver.

        this._initiatedByBackendStop = true;

        // FIXME: This is purely to avoid a noisy assert. Previously
        // it was impossible to stop without stopping from the UI.
        console.assert(!this._isCapturing);
        this._isCapturing = true;

        this.stopCapturing();
    }

    eventRecorded(recordPayload)
    {
        // Called from WI.TimelineObserver.

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
            if (record.type === WI.TimelineRecord.Type.RenderingFrame) {
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
        // Called from WI.PageObserver.

        console.assert(this._activeRecording);
        console.assert(isNaN(WI.networkManager.mainFrame.domContentReadyEventTimestamp));

        let computedTimestamp = this.activeRecording.computeElapsedTime(timestamp);

        WI.networkManager.mainFrame.markDOMContentReadyEvent(computedTimestamp);

        let eventMarker = new WI.TimelineMarker(computedTimestamp, WI.TimelineMarker.Type.DOMContentEvent);
        this._activeRecording.addEventMarker(eventMarker);
    }

    pageLoadEventFired(timestamp)
    {
        // Called from WI.PageObserver.

        console.assert(this._activeRecording);
        console.assert(isNaN(WI.networkManager.mainFrame.loadEventTimestamp));

        let computedTimestamp = this.activeRecording.computeElapsedTime(timestamp);

        WI.networkManager.mainFrame.markLoadEvent(computedTimestamp);

        let eventMarker = new WI.TimelineMarker(computedTimestamp, WI.TimelineMarker.Type.LoadEvent);
        this._activeRecording.addEventMarker(eventMarker);

        this._stopAutoRecordingSoon();
    }

    cpuProfilerTrackingStarted(timestamp)
    {
        // Called from WI.CPUProfilerObserver.

        this.capturingStarted(timestamp);
    }

    cpuProfilerTrackingUpdated(event)
    {
        // Called from WI.CPUProfilerObserver.

        if (!this._isCapturing)
            return;

        this._addRecord(new WI.CPUTimelineRecord(event));
    }

    cpuProfilerTrackingCompleted()
    {
        // Called from WI.CPUProfilerObserver.
    }

    memoryTrackingStarted(timestamp)
    {
        // Called from WI.MemoryObserver.

        this.capturingStarted(timestamp);
    }

    memoryTrackingUpdated(event)
    {
        // Called from WI.MemoryObserver.

        if (!this._isCapturing)
            return;

        this._addRecord(new WI.MemoryTimelineRecord(event.timestamp, event.categories));
    }

    memoryTrackingCompleted()
    {
        // Called from WI.MemoryObserver.
    }

    heapTrackingStarted(timestamp, snapshot)
    {
        // Called from WI.HeapObserver.

        this._addRecord(new WI.HeapAllocationsTimelineRecord(timestamp, snapshot));

        this.capturingStarted(timestamp);
    }

    heapTrackingCompleted(timestamp, snapshot)
    {
        // Called from WI.HeapObserver.

        this._addRecord(new WI.HeapAllocationsTimelineRecord(timestamp, snapshot));
    }

    heapSnapshotAdded(timestamp, snapshot)
    {
        // Called from WI.HeapAllocationsInstrument.

        this._addRecord(new WI.HeapAllocationsTimelineRecord(timestamp, snapshot));
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
            return new WI.LayoutTimelineRecord(WI.LayoutTimelineRecord.EventType.InvalidateStyles, startTime, startTime, callFrames, sourceCodeLocation);

        case TimelineAgent.EventType.RecalculateStyles:
            return new WI.LayoutTimelineRecord(WI.LayoutTimelineRecord.EventType.RecalculateStyles, startTime, endTime, callFrames, sourceCodeLocation);

        case TimelineAgent.EventType.InvalidateLayout:
            console.assert(isNaN(endTime));

            // Pass the startTime as the endTime since this record type has no duration.
            return new WI.LayoutTimelineRecord(WI.LayoutTimelineRecord.EventType.InvalidateLayout, startTime, startTime, callFrames, sourceCodeLocation);

        case TimelineAgent.EventType.Layout:
            var layoutRecordType = sourceCodeLocation ? WI.LayoutTimelineRecord.EventType.ForcedLayout : WI.LayoutTimelineRecord.EventType.Layout;
            var quad = new WI.Quad(recordPayload.data.root);
            return new WI.LayoutTimelineRecord(layoutRecordType, startTime, endTime, callFrames, sourceCodeLocation, quad);

        case TimelineAgent.EventType.Paint:
            var quad = new WI.Quad(recordPayload.data.clip);
            return new WI.LayoutTimelineRecord(WI.LayoutTimelineRecord.EventType.Paint, startTime, endTime, callFrames, sourceCodeLocation, quad);

        case TimelineAgent.EventType.Composite:
            return new WI.LayoutTimelineRecord(WI.LayoutTimelineRecord.EventType.Composite, startTime, endTime, callFrames, sourceCodeLocation);

        case TimelineAgent.EventType.RenderingFrame:
            if (!recordPayload.children || !recordPayload.children.length)
                return null;

            return new WI.RenderingFrameTimelineRecord(startTime, endTime);

        case TimelineAgent.EventType.EvaluateScript:
            if (!sourceCodeLocation) {
                var mainFrame = WI.networkManager.mainFrame;
                var scriptResource = mainFrame.url === recordPayload.data.url ? mainFrame.mainResource : mainFrame.resourceForURL(recordPayload.data.url, true);
                if (scriptResource) {
                    // The lineNumber is 1-based, but we expect 0-based.
                    let lineNumber = recordPayload.data.lineNumber - 1;
                    let columnNumber = "columnNumber" in recordPayload.data ? recordPayload.data.columnNumber - 1 : 0;
                    sourceCodeLocation = scriptResource.createSourceCodeLocation(lineNumber, columnNumber);
                }
            }

            var profileData = recordPayload.data.profile;

            var record;
            switch (parentRecordPayload && parentRecordPayload.type) {
            case TimelineAgent.EventType.TimerFire:
                record = new WI.ScriptTimelineRecord(WI.ScriptTimelineRecord.EventType.TimerFired, startTime, endTime, callFrames, sourceCodeLocation, parentRecordPayload.data.timerId, profileData);
                break;
            case TimelineAgent.EventType.ObserverCallback:
                record = new WI.ScriptTimelineRecord(WI.ScriptTimelineRecord.EventType.ObserverCallback, startTime, endTime, callFrames, sourceCodeLocation, parentRecordPayload.data.type, profileData);
                break;
            case TimelineAgent.EventType.FireAnimationFrame:
                record = new WI.ScriptTimelineRecord(WI.ScriptTimelineRecord.EventType.AnimationFrameFired, startTime, endTime, callFrames, sourceCodeLocation, parentRecordPayload.data.id, profileData);
                break;
            default:
                record = new WI.ScriptTimelineRecord(WI.ScriptTimelineRecord.EventType.ScriptEvaluated, startTime, endTime, callFrames, sourceCodeLocation, null, profileData);
                break;
            }

            this._webTimelineScriptRecordsExpectingScriptProfilerEvents.push(record);
            return record;

        case TimelineAgent.EventType.ConsoleProfile:
            var profileData = recordPayload.data.profile;
            // COMPATIBILITY (iOS 9): With the Sampling Profiler, profiles no longer include legacy profile data.
            console.assert(profileData || TimelineAgent.setInstruments);
            return new WI.ScriptTimelineRecord(WI.ScriptTimelineRecord.EventType.ConsoleProfileRecorded, startTime, endTime, callFrames, sourceCodeLocation, recordPayload.data.title, profileData);

        case TimelineAgent.EventType.TimerFire:
        case TimelineAgent.EventType.EventDispatch:
        case TimelineAgent.EventType.FireAnimationFrame:
        case TimelineAgent.EventType.ObserverCallback:
            // These are handled when we see the child FunctionCall or EvaluateScript.
            break;

        case TimelineAgent.EventType.FunctionCall:
            // FunctionCall always happens as a child of another record, and since the FunctionCall record
            // has useful info we just make the timeline record here (combining the data from both records).
            if (!parentRecordPayload) {
                console.warn("Unexpectedly received a FunctionCall timeline record without a parent record");
                break;
            }

            if (!sourceCodeLocation) {
                var mainFrame = WI.networkManager.mainFrame;
                var scriptResource = mainFrame.url === recordPayload.data.scriptName ? mainFrame.mainResource : mainFrame.resourceForURL(recordPayload.data.scriptName, true);
                if (scriptResource) {
                    // The lineNumber is 1-based, but we expect 0-based.
                    let lineNumber = recordPayload.data.scriptLine - 1;
                    let columnNumber = "scriptColumn" in recordPayload.data ? recordPayload.data.scriptColumn - 1 : 0;
                    sourceCodeLocation = scriptResource.createSourceCodeLocation(lineNumber, columnNumber);
                }
            }

            var profileData = recordPayload.data.profile;

            var record;
            switch (parentRecordPayload.type) {
            case TimelineAgent.EventType.TimerFire:
                record = new WI.ScriptTimelineRecord(WI.ScriptTimelineRecord.EventType.TimerFired, startTime, endTime, callFrames, sourceCodeLocation, parentRecordPayload.data.timerId, profileData);
                break;
            case TimelineAgent.EventType.EventDispatch:
                record = new WI.ScriptTimelineRecord(WI.ScriptTimelineRecord.EventType.EventDispatched, startTime, endTime, callFrames, sourceCodeLocation, parentRecordPayload.data.type, profileData);
                break;
            case TimelineAgent.EventType.ObserverCallback:
                record = new WI.ScriptTimelineRecord(WI.ScriptTimelineRecord.EventType.ObserverCallback, startTime, endTime, callFrames, sourceCodeLocation, parentRecordPayload.data.type, profileData);
                break;
            case TimelineAgent.EventType.FireAnimationFrame:
                record = new WI.ScriptTimelineRecord(WI.ScriptTimelineRecord.EventType.AnimationFrameFired, startTime, endTime, callFrames, sourceCodeLocation, parentRecordPayload.data.id, profileData);
                break;
            case TimelineAgent.EventType.FunctionCall:
                record = new WI.ScriptTimelineRecord(WI.ScriptTimelineRecord.EventType.ScriptEvaluated, startTime, endTime, callFrames, sourceCodeLocation, parentRecordPayload.data.id, profileData);
                break;
            case TimelineAgent.EventType.RenderingFrame:
                record = new WI.ScriptTimelineRecord(WI.ScriptTimelineRecord.EventType.ScriptEvaluated, startTime, endTime, callFrames, sourceCodeLocation, parentRecordPayload.data.id, profileData);
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
            sourceCodeLocation = WI.debuggerManager.probeForIdentifier(recordPayload.data.probeId).breakpoint.sourceCodeLocation;
            return new WI.ScriptTimelineRecord(WI.ScriptTimelineRecord.EventType.ProbeSampleRecorded, startTime, startTime, callFrames, sourceCodeLocation, recordPayload.data.probeId);

        case TimelineAgent.EventType.TimerInstall:
            console.assert(isNaN(endTime));

            // Pass the startTime as the endTime since this record type has no duration.
            var timerDetails = {timerId: recordPayload.data.timerId, timeout: recordPayload.data.timeout, repeating: !recordPayload.data.singleShot};
            return new WI.ScriptTimelineRecord(WI.ScriptTimelineRecord.EventType.TimerInstalled, startTime, startTime, callFrames, sourceCodeLocation, timerDetails);

        case TimelineAgent.EventType.TimerRemove:
            console.assert(isNaN(endTime));

            // Pass the startTime as the endTime since this record type has no duration.
            return new WI.ScriptTimelineRecord(WI.ScriptTimelineRecord.EventType.TimerRemoved, startTime, startTime, callFrames, sourceCodeLocation, recordPayload.data.timerId);

        case TimelineAgent.EventType.RequestAnimationFrame:
            console.assert(isNaN(endTime));

            // Pass the startTime as the endTime since this record type has no duration.
            return new WI.ScriptTimelineRecord(WI.ScriptTimelineRecord.EventType.AnimationFrameRequested, startTime, startTime, callFrames, sourceCodeLocation, recordPayload.data.id);

        case TimelineAgent.EventType.CancelAnimationFrame:
            console.assert(isNaN(endTime));

            // Pass the startTime as the endTime since this record type has no duration.
            return new WI.ScriptTimelineRecord(WI.ScriptTimelineRecord.EventType.AnimationFrameCanceled, startTime, startTime, callFrames, sourceCodeLocation, recordPayload.data.id);

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
            var eventMarker = new WI.TimelineMarker(timestamp, WI.TimelineMarker.Type.TimeStamp, recordPayload.data.message);
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

        let instruments = this.enabledTimelineTypes.map((type) => WI.Instrument.createForTimelineType(type));
        let identifier = this._nextRecordingIdentifier++;
        let newRecording = new WI.TimelineRecording(identifier, WI.UIString("Timeline Recording %d").format(identifier), instruments);

        this._recordings.push(newRecording);
        this.dispatchEventToListeners(WI.TimelineManager.Event.RecordingCreated, {recording: newRecording});

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
        if (this._mainResourceForAutoCapturing && WI.TimelineRecording.isLegacy) {
            console.assert(this._mainResourceForAutoCapturing.originalRequestWillBeSentTimestamp);
            this._activeRecording.setLegacyBaseTimestamp(this._mainResourceForAutoCapturing.originalRequestWillBeSentTimestamp);
            this._mainResourceForAutoCapturing._requestSentTimestamp = 0;
        }

        this.dispatchEventToListeners(WI.TimelineManager.Event.RecordingLoaded, {oldRecording});
    }

    _callFramesFromPayload(payload)
    {
        if (!payload)
            return null;

        return payload.map((x) => WI.CallFrame.fromPayload(WI.assumingMainTarget(), x));
    }

    _addRecord(record)
    {
        this._activeRecording.addRecord(record);

        // Only worry about dead time after the load event.
        if (WI.networkManager.mainFrame && isNaN(WI.networkManager.mainFrame.loadEventTimestamp))
            this._resetAutoRecordingDeadTimeTimeout();
    }

    _attemptAutoCapturingForFrame(frame)
    {
        if (!this._autoCaptureOnPageLoad)
            return false;

        if (!frame.isMainFrame())
            return false;

        // COMPATIBILITY (iOS 9): Timeline.setAutoCaptureEnabled did not exist.
        // Perform auto capture in the frontend.
        if (!window.TimelineAgent)
            return false;
        if (!TimelineAgent.setAutoCaptureEnabled)
            return this._legacyAttemptStartAutoCapturingForFrame(frame);

        if (!this._shouldSetAutoCapturingMainResource)
            return false;

        console.assert(this._isCapturing, "We saw autoCaptureStarted so we should already be capturing");

        let mainResource = frame.provisionalMainResource || frame.mainResource;
        if (mainResource === this._mainResourceForAutoCapturing)
            return false;

        let oldMainResource = frame.mainResource || null;
        this._isCapturingPageReload = oldMainResource !== null && oldMainResource.url === mainResource.url;

        this._mainResourceForAutoCapturing = mainResource;

        this._addRecord(new WI.ResourceTimelineRecord(mainResource));

        this._resetAutoRecordingMaxTimeTimeout();

        this._shouldSetAutoCapturingMainResource = false;

        return true;
    }

    _legacyAttemptStartAutoCapturingForFrame(frame)
    {
        if (this._isCapturing && !this._mainResourceForAutoCapturing)
            return false;

        let mainResource = frame.provisionalMainResource || frame.mainResource;
        if (mainResource === this._mainResourceForAutoCapturing)
            return false;

        let oldMainResource = frame.mainResource || null;
        this._isCapturingPageReload = oldMainResource !== null && oldMainResource.url === mainResource.url;

        if (this._isCapturing)
            this.stopCapturing();

        this._mainResourceForAutoCapturing = mainResource;

        this._loadNewRecording();

        this.startCapturing();

        this._addRecord(new WI.ResourceTimelineRecord(mainResource));

        this._resetAutoRecordingMaxTimeTimeout();

        return true;
    }

    _stopAutoRecordingSoon()
    {
        if (!WI.settings.timelinesAutoStop.value)
            return;

        // Only auto stop when auto capturing.
        if (!this._isCapturing || !this._mainResourceForAutoCapturing)
            return;

        if (this._stopCapturingTimeout)
            clearTimeout(this._stopCapturingTimeout);
        this._stopCapturingTimeout = setTimeout(this._boundStopCapturing, WI.TimelineManager.MaximumAutoRecordDurationAfterLoadEvent);
    }

    _resetAutoRecordingMaxTimeTimeout()
    {
        if (!WI.settings.timelinesAutoStop.value)
            return;

        if (this._stopCapturingTimeout)
            clearTimeout(this._stopCapturingTimeout);
        this._stopCapturingTimeout = setTimeout(this._boundStopCapturing, WI.TimelineManager.MaximumAutoRecordDuration);
    }

    _resetAutoRecordingDeadTimeTimeout()
    {
        if (!WI.settings.timelinesAutoStop.value)
            return;

        // Only monitor dead time when auto capturing.
        if (!this._isCapturing || !this._mainResourceForAutoCapturing)
            return;

        // Avoid unnecessary churning of timeout identifier by not tickling until 10ms have passed.
        let now = Date.now();
        if (now <= this._lastDeadTimeTickle)
            return;
        this._lastDeadTimeTickle = now + 10;

        if (this._deadTimeTimeout)
            clearTimeout(this._deadTimeTimeout);
        this._deadTimeTimeout = setTimeout(this._boundStopCapturing, WI.TimelineManager.DeadTimeRequiredToStopAutoRecordingEarly);
    }

    _provisionalLoadStarted(event)
    {
        this._attemptAutoCapturingForFrame(event.target);
    }

    _mainResourceDidChange(event)
    {
        // Ignore resource events when there isn't a main frame yet. Those events are triggered by
        // loading the cached resources when the inspector opens, and they do not have timing information.
        if (!WI.networkManager.mainFrame)
            return;

        let frame = event.target;

        // When performing a page transition start a recording once the main resource changes.
        // We start a legacy capture because the backend wasn't available to automatically
        // initiate the capture, so the frontend must start the capture.
        if (this._transitioningPageTarget) {
            this._transitioningPageTarget = false;
            if (this._autoCaptureOnPageLoad)
                this._legacyAttemptStartAutoCapturingForFrame(frame);
            return;
        }

        if (this._attemptAutoCapturingForFrame(frame))
            return;

        if (!this._isCapturing)
            return;

        let mainResource = frame.mainResource;
        if (mainResource === this._mainResourceForAutoCapturing)
            return;

        this._addRecord(new WI.ResourceTimelineRecord(mainResource));
    }

    _resourceWasAdded(event)
    {

        // Ignore resource events when there isn't a main frame yet. Those events are triggered by
        // loading the cached resources when the inspector opens, and they do not have timing information.
        if (!WI.networkManager.mainFrame)
            return;

        if (!this._isCapturing)
            return;

        this._addRecord(new WI.ResourceTimelineRecord(event.data.resource));
    }

    _garbageCollected(event)
    {
        if (!this._isCapturing)
            return;

        let collection = event.data.collection;
        this._addRecord(new WI.ScriptTimelineRecord(WI.ScriptTimelineRecord.EventType.GarbageCollected, collection.startTime, collection.endTime, null, null, collection));
    }

    _memoryPressure(event)
    {
        if (!this._isCapturing)
            return;

        this.activeRecording.addMemoryPressureEvent(event.data.memoryPressureEvent);
    }

    _handleTimelinesAutoStopSettingChanged(event)
    {
        if (!this._isCapturing)
            return;

        if (WI.settings.timelinesAutoStop.value) {
            if (this._mainResourceForAutoCapturing && !isNaN(this._mainResourceForAutoCapturing.parentFrame.loadEventTimestamp))
                this._stopAutoRecordingSoon();
            else
                this._resetAutoRecordingMaxTimeTimeout();
            this._resetAutoRecordingDeadTimeTimeout();
        } else
            this.relaxAutoStop();
    }

    _scriptProfilerTypeToScriptTimelineRecordType(type)
    {
        switch (type) {
        case ScriptProfilerAgent.EventType.API:
            return WI.ScriptTimelineRecord.EventType.APIScriptEvaluated;
        case ScriptProfilerAgent.EventType.Microtask:
            return WI.ScriptTimelineRecord.EventType.MicrotaskDispatched;
        case ScriptProfilerAgent.EventType.Other:
            return WI.ScriptTimelineRecord.EventType.ScriptEvaluated;
        }
    }

    scriptProfilerProgrammaticCaptureStarted()
    {
        // FIXME: <https://webkit.org/b/158753> Generalize the concept of Instruments on the backend to work equally for JSContext and Web inspection
        console.assert(WI.sharedApp.debuggableType === WI.DebuggableType.JavaScript);
        console.assert(!this._isCapturing);

        this.programmaticCaptureStarted();
    }

    scriptProfilerProgrammaticCaptureStopped()
    {
        // FIXME: <https://webkit.org/b/158753> Generalize the concept of Instruments on the backend to work equally for JSContext and Web inspection
        console.assert(WI.sharedApp.debuggableType === WI.DebuggableType.JavaScript);
        console.assert(this._isCapturing);

        this.programmaticCaptureStopped();
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
        let record = new WI.ScriptTimelineRecord(scriptRecordType, startTime, endTime, null, null, null, null);
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
            let {stackTraces} = samples;
            let topDownCallingContextTree = this.activeRecording.topDownCallingContextTree;
            let bottomUpCallingContextTree = this.activeRecording.bottomUpCallingContextTree;
            let topFunctionsTopDownCallingContextTree = this.activeRecording.topFunctionsTopDownCallingContextTree;
            let topFunctionsBottomUpCallingContextTree = this.activeRecording.topFunctionsBottomUpCallingContextTree;

            // Calculate a per-sample duration.
            let timestampIndex = 0;
            let timestampCount = stackTraces.length;
            let sampleDurations = new Array(timestampCount);
            let sampleDurationIndex = 0;
            const defaultDuration = 1 / 1000; // 1ms.
            for (let i = 0; i < this._scriptProfilerRecords.length; ++i) {
                let record = this._scriptProfilerRecords[i];

                // Use a default duration for timestamps recorded outside of ScriptProfiler events.
                while (timestampIndex < timestampCount && stackTraces[timestampIndex].timestamp < record.startTime) {
                    sampleDurations[sampleDurationIndex++] = defaultDuration;
                    timestampIndex++;
                }

                // Average the duration per sample across all samples during the record.
                let samplesInRecord = 0;
                while (timestampIndex < timestampCount && stackTraces[timestampIndex].timestamp < record.endTime) {
                    timestampIndex++;
                    samplesInRecord++;
                }
                if (samplesInRecord) {
                    let averageDuration = (record.endTime - record.startTime) / samplesInRecord;
                    sampleDurations.fill(averageDuration, sampleDurationIndex, sampleDurationIndex + samplesInRecord);
                    sampleDurationIndex += samplesInRecord;
                }
            }

            // Use a default duration for timestamps recorded outside of ScriptProfiler events.
            if (timestampIndex < timestampCount)
                sampleDurations.fill(defaultDuration, sampleDurationIndex);

            for (let i = 0; i < stackTraces.length; i++) {
                topDownCallingContextTree.updateTreeWithStackTrace(stackTraces[i], sampleDurations[i]);
                bottomUpCallingContextTree.updateTreeWithStackTrace(stackTraces[i], sampleDurations[i]);
                topFunctionsTopDownCallingContextTree.updateTreeWithStackTrace(stackTraces[i], sampleDurations[i]);
                topFunctionsBottomUpCallingContextTree.updateTreeWithStackTrace(stackTraces[i], sampleDurations[i]);
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
        if (WI.sharedApp.debuggableType !== WI.DebuggableType.JavaScript) {
            this._scriptProfilerRecords = this._scriptProfilerRecords.filter((x) => x.__scriptProfilerType === ScriptProfilerAgent.EventType.Other);
            this._mergeScriptProfileRecords();
        }

        this._scriptProfilerRecords = null;

        let timeline = this.activeRecording.timelineForRecordType(WI.TimelineRecord.Type.Script);
        timeline.refresh();
    }

    _mergeScriptProfileRecords()
    {
        let nextRecord = function(list) { return list.shift() || null; };
        let nextWebTimelineRecord = nextRecord.bind(null, this._webTimelineScriptRecordsExpectingScriptProfilerEvents);
        let nextScriptProfilerRecord = nextRecord.bind(null, this._scriptProfilerRecords);
        let recordEnclosesRecord = function(record1, record2) {
            return record1.startTime <= record2.startTime && record1.endTime >= record2.endTime;
        };

        let webRecord = nextWebTimelineRecord();
        let profilerRecord = nextScriptProfilerRecord();

        while (webRecord && profilerRecord) {
            // Skip web records with parent web records. For example an EvaluateScript with an EvaluateScript parent.
            if (webRecord.parent instanceof WI.ScriptTimelineRecord) {
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

    _updateAutoCaptureInstruments(targets)
    {
        let enabledTimelineTypes = this._enabledTimelineTypesSetting.value;

        for (let target of targets) {
            if (!target.TimelineAgent)
                continue;
            if (!target.TimelineAgent.setInstruments)
                continue;

            let instrumentSet = new Set;
            for (let timelineType of enabledTimelineTypes) {
                switch (timelineType) {
                case WI.TimelineRecord.Type.Script:
                    instrumentSet.add(target.TimelineAgent.Instrument.ScriptProfiler);
                    break;
                case WI.TimelineRecord.Type.HeapAllocations:
                    instrumentSet.add(target.TimelineAgent.Instrument.Heap);
                    break;
                case WI.TimelineRecord.Type.Network:
                case WI.TimelineRecord.Type.RenderingFrame:
                case WI.TimelineRecord.Type.Layout:
                case WI.TimelineRecord.Type.Media:
                    instrumentSet.add(target.TimelineAgent.Instrument.Timeline);
                    break;
                case WI.TimelineRecord.Type.CPU:
                    instrumentSet.add(target.TimelineAgent.Instrument.CPU);
                    break;
                case WI.TimelineRecord.Type.Memory:
                    instrumentSet.add(target.TimelineAgent.Instrument.Memory);
                    break;
                }
            }

            target.TimelineAgent.setInstruments(Array.from(instrumentSet));
        }
    }

    _handleDOMNodeDidFireEvent(event)
    {
        console.assert(this._isCapturing);

        let {domEvent} = event.data;

        this._addRecord(new WI.MediaTimelineRecord(WI.MediaTimelineRecord.EventType.DOMEvent, domEvent.timestamp, {
            domNode: event.target,
            domEvent,
        }));
    }

    _handleDOMNodeLowPowerChanged(event)
    {
        console.assert(this._isCapturing);

        let {timestamp, isLowPower} = event.data;

        this._addRecord(new WI.MediaTimelineRecord(WI.MediaTimelineRecord.EventType.LowPower, timestamp, {
            domNode: event.target,
            isLowPower,
        }));
    }
};

WI.TimelineManager.Event = {
    RecordingCreated: "timeline-manager-recording-created",
    RecordingLoaded: "timeline-manager-recording-loaded",
    CapturingWillStart: "timeline-manager-capturing-will-start",
    CapturingStarted: "timeline-manager-capturing-started",
    CapturingStopped: "timeline-manager-capturing-stopped"
};

WI.TimelineManager.MaximumAutoRecordDuration = 90000; // 90 seconds
WI.TimelineManager.MaximumAutoRecordDurationAfterLoadEvent = 10000; // 10 seconds
WI.TimelineManager.DeadTimeRequiredToStopAutoRecordingEarly = 2000; // 2 seconds
