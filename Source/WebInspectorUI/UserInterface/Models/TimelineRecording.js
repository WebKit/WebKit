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

WI.TimelineRecording = class TimelineRecording extends WI.Object
{
    constructor(identifier, displayName, instruments)
    {
        super();

        this._identifier = identifier;
        this._timelines = new Map;
        this._displayName = displayName;
        this._capturing = false;
        this._readonly = false;
        this._instruments = instruments || [];
        this._topDownCallingContextTree = new WI.CallingContextTree(WI.CallingContextTree.Type.TopDown);
        this._bottomUpCallingContextTree = new WI.CallingContextTree(WI.CallingContextTree.Type.BottomUp);
        this._topFunctionsTopDownCallingContextTree = new WI.CallingContextTree(WI.CallingContextTree.Type.TopFunctionsTopDown);
        this._topFunctionsBottomUpCallingContextTree = new WI.CallingContextTree(WI.CallingContextTree.Type.TopFunctionsBottomUp);

        for (let type of WI.TimelineManager.availableTimelineTypes()) {
            let timeline = WI.Timeline.create(type);
            this._timelines.set(type, timeline);
            timeline.addEventListener(WI.Timeline.Event.TimesUpdated, this._timelineTimesUpdated, this);
        }

        // For legacy backends, we compute the elapsed time of records relative to this timestamp.
        this._legacyFirstRecordedTimestamp = NaN;

        this.reset(true);
    }

    // Static

    static sourceCodeTimelinesSupported()
    {
        // FIXME: Support Network Timeline in ServiceWorker.
        return WI.sharedApp.debuggableType === WI.DebuggableType.Web;
    }

    // Public

    get displayName() { return this._displayName; }
    get identifier() { return this._identifier; }
    get timelines() { return this._timelines; }
    get instruments() { return this._instruments; }
    get readonly() { return this._readonly; }
    get startTime() { return this._startTime; }
    get endTime() { return this._endTime; }

    get topDownCallingContextTree() { return this._topDownCallingContextTree; }
    get bottomUpCallingContextTree() { return this._bottomUpCallingContextTree; }
    get topFunctionsTopDownCallingContextTree() { return this._topFunctionsTopDownCallingContextTree; }
    get topFunctionsBottomUpCallingContextTree() { return this._topFunctionsBottomUpCallingContextTree; }

    start(initiatedByBackend)
    {
        console.assert(!this._capturing, "Attempted to start an already started session.");
        console.assert(!this._readonly, "Attempted to start a readonly session.");

        this._capturing = true;

        for (let instrument of this._instruments)
            instrument.startInstrumentation(initiatedByBackend);
    }

    stop(initiatedByBackend)
    {
        console.assert(this._capturing, "Attempted to stop an already stopped session.");
        console.assert(!this._readonly, "Attempted to stop a readonly session.");

        this._capturing = false;

        for (let instrument of this._instruments)
            instrument.stopInstrumentation(initiatedByBackend);
    }

    saveIdentityToCookie()
    {
        // Do nothing. Timeline recordings are not persisted when the inspector is
        // re-opened, so do not attempt to restore by identifier or display name.
    }

    isEmpty()
    {
        for (var timeline of this._timelines.values()) {
            if (timeline.records.length)
                return false;
        }

        return true;
    }

    unloaded()
    {
        console.assert(!this.isEmpty(), "Shouldn't unload an empty recording; it should be reused instead.");

        this._readonly = true;

        this.dispatchEventToListeners(WI.TimelineRecording.Event.Unloaded);
    }

    reset(suppressEvents)
    {
        console.assert(!this._readonly, "Can't reset a read-only recording.");

        this._sourceCodeTimelinesMap = new Map;
        this._startTime = NaN;
        this._endTime = NaN;
        this._discontinuities = [];

        this._topDownCallingContextTree.reset();
        this._bottomUpCallingContextTree.reset();
        this._topFunctionsTopDownCallingContextTree.reset();
        this._topFunctionsBottomUpCallingContextTree.reset();

        for (var timeline of this._timelines.values())
            timeline.reset(suppressEvents);

        WI.RenderingFrameTimelineRecord.resetFrameIndex();

        if (!suppressEvents) {
            this.dispatchEventToListeners(WI.TimelineRecording.Event.Reset);
            this.dispatchEventToListeners(WI.TimelineRecording.Event.TimesUpdated);
        }
    }

    sourceCodeTimelinesForSourceCode(sourceCode)
    {
        var timelines = this._sourceCodeTimelinesMap.get(sourceCode);
        if (!timelines)
            return [];
        return [...timelines.values()];
    }

    timelineForInstrument(instrument)
    {
        return this._timelines.get(instrument.timelineRecordType);
    }

    instrumentForTimeline(timeline)
    {
        return this._instruments.find((instrument) => instrument.timelineRecordType === timeline.type);
    }

    timelineForRecordType(recordType)
    {
        return this._timelines.get(recordType);
    }

    addInstrument(instrument)
    {
        console.assert(instrument instanceof WI.Instrument, instrument);
        console.assert(!this._instruments.includes(instrument), this._instruments, instrument);

        this._instruments.push(instrument);

        this.dispatchEventToListeners(WI.TimelineRecording.Event.InstrumentAdded, {instrument});
    }

    removeInstrument(instrument)
    {
        console.assert(instrument instanceof WI.Instrument, instrument);
        console.assert(this._instruments.includes(instrument), this._instruments, instrument);

        this._instruments.remove(instrument);

        this.dispatchEventToListeners(WI.TimelineRecording.Event.InstrumentRemoved, {instrument});
    }

    addEventMarker(marker)
    {
        if (!this._capturing)
            return;

        this.dispatchEventToListeners(WI.TimelineRecording.Event.MarkerAdded, {marker});
    }

    addRecord(record)
    {
        var timeline = this._timelines.get(record.type);
        console.assert(timeline, record, this._timelines);
        if (!timeline)
            return;

        // Add the record to the global timeline by type.
        timeline.addRecord(record);

        // Some records don't have source code timelines.
        if (record.type === WI.TimelineRecord.Type.Network
            || record.type === WI.TimelineRecord.Type.RenderingFrame
            || record.type === WI.TimelineRecord.Type.Memory
            || record.type === WI.TimelineRecord.Type.HeapAllocations
            || record.type === WI.TimelineRecord.Type.Media)
            return;

        if (!WI.TimelineRecording.sourceCodeTimelinesSupported())
            return;

        // Add the record to the source code timelines.
        var activeMainResource = WI.networkManager.mainFrame.provisionalMainResource || WI.networkManager.mainFrame.mainResource;
        var sourceCode = record.sourceCodeLocation ? record.sourceCodeLocation.sourceCode : activeMainResource;

        var sourceCodeTimelines = this._sourceCodeTimelinesMap.get(sourceCode);
        if (!sourceCodeTimelines) {
            sourceCodeTimelines = new Map;
            this._sourceCodeTimelinesMap.set(sourceCode, sourceCodeTimelines);
        }

        var newTimeline = false;
        var key = this._keyForRecord(record);
        var sourceCodeTimeline = sourceCodeTimelines.get(key);
        if (!sourceCodeTimeline) {
            sourceCodeTimeline = new WI.SourceCodeTimeline(sourceCode, record.sourceCodeLocation, record.type, record.eventType);
            sourceCodeTimelines.set(key, sourceCodeTimeline);
            newTimeline = true;
        }

        sourceCodeTimeline.addRecord(record);

        if (newTimeline)
            this.dispatchEventToListeners(WI.TimelineRecording.Event.SourceCodeTimelineAdded, {sourceCodeTimeline});
    }

    addMemoryPressureEvent(memoryPressureEvent)
    {
        let memoryTimeline = this._timelines.get(WI.TimelineRecord.Type.Memory);
        console.assert(memoryTimeline, this._timelines);
        if (!memoryTimeline)
            return;

        memoryTimeline.addMemoryPressureEvent(memoryPressureEvent);
    }

    addDiscontinuity(startTime, endTime)
    {
        this._discontinuities.push({startTime, endTime});
    }

    discontinuitiesInTimeRange(startTime, endTime)
    {
        return this._discontinuities.filter((item) => item.startTime < endTime && item.endTime > startTime);
    }

    addScriptInstrumentForProgrammaticCapture()
    {
        for (let instrument of this._instruments) {
            if (instrument instanceof WI.ScriptInstrument)
                return;
        }

        this.addInstrument(new WI.ScriptInstrument);

        let instrumentTypes = this._instruments.map((instrument) => instrument.timelineRecordType);
        WI.timelineManager.enabledTimelineTypes = instrumentTypes;
    }

    computeElapsedTime(timestamp)
    {
        if (!timestamp || isNaN(timestamp))
            return NaN;

        // COMPATIBILITY (iOS 8): old backends send timestamps (seconds or milliseconds since the epoch),
        // rather than seconds elapsed since timeline capturing started. We approximate the latter by
        // subtracting the start timestamp, as old versions did not use monotonic times.
        if (WI.TimelineRecording.isLegacy === undefined)
            WI.TimelineRecording.isLegacy = timestamp > WI.TimelineRecording.TimestampThresholdForLegacyRecordConversion;

        if (!WI.TimelineRecording.isLegacy)
            return timestamp;

        // If the record's start time is large, but not really large, then it is seconds since epoch
        // not millseconds since epoch, so convert it to milliseconds.
        if (timestamp < WI.TimelineRecording.TimestampThresholdForLegacyAssumedMilliseconds)
            timestamp *= 1000;

        if (isNaN(this._legacyFirstRecordedTimestamp))
            this._legacyFirstRecordedTimestamp = timestamp;

        // Return seconds since the first recorded value.
        return (timestamp - this._legacyFirstRecordedTimestamp) / 1000.0;
    }

    setLegacyBaseTimestamp(timestamp)
    {
        console.assert(isNaN(this._legacyFirstRecordedTimestamp));

        if (timestamp < WI.TimelineRecording.TimestampThresholdForLegacyAssumedMilliseconds)
            timestamp *= 1000;

        this._legacyFirstRecordedTimestamp = timestamp;
    }

    initializeTimeBoundsIfNecessary(timestamp)
    {
        if (isNaN(this._startTime)) {
            console.assert(isNaN(this._endTime));

            this._startTime = timestamp;
            this._endTime = timestamp;

            this.dispatchEventToListeners(WI.TimelineRecording.Event.TimesUpdated);
        }
    }

    // Private

    _keyForRecord(record)
    {
        var key = record.type;
        if (record instanceof WI.ScriptTimelineRecord || record instanceof WI.LayoutTimelineRecord || record instanceof WI.MediaTimelineRecord)
            key += ":" + record.eventType;
        if (record instanceof WI.ScriptTimelineRecord && record.eventType === WI.ScriptTimelineRecord.EventType.EventDispatched)
            key += ":" + record.details;
        if (record.sourceCodeLocation)
            key += ":" + record.sourceCodeLocation.lineNumber + ":" + record.sourceCodeLocation.columnNumber;
        return key;
    }

    _timelineTimesUpdated(event)
    {
        var timeline = event.target;
        var changed = false;

        if (isNaN(this._startTime) || timeline.startTime < this._startTime) {
            this._startTime = timeline.startTime;
            changed = true;
        }

        if (isNaN(this._endTime) || this._endTime < timeline.endTime) {
            this._endTime = timeline.endTime;
            changed = true;
        }

        if (changed)
            this.dispatchEventToListeners(WI.TimelineRecording.Event.TimesUpdated);
    }
};

WI.TimelineRecording.Event = {
    Reset: "timeline-recording-reset",
    Unloaded: "timeline-recording-unloaded",
    SourceCodeTimelineAdded: "timeline-recording-source-code-timeline-added",
    InstrumentAdded: "timeline-recording-instrument-added",
    InstrumentRemoved: "timeline-recording-instrument-removed",
    TimesUpdated: "timeline-recording-times-updated",
    MarkerAdded: "timeline-recording-marker-added",
};

WI.TimelineRecording.isLegacy = undefined;
WI.TimelineRecording.TimestampThresholdForLegacyRecordConversion = 10000000; // Some value not near zero.
WI.TimelineRecording.TimestampThresholdForLegacyAssumedMilliseconds = 1420099200000; // Date.parse("Jan 1, 2015"). Milliseconds since epoch.
