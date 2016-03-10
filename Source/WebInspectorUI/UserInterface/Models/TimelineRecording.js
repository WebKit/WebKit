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

WebInspector.TimelineRecording = class TimelineRecording extends WebInspector.Object
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
        this._topDownCallingContextTree = new WebInspector.CallingContextTree(WebInspector.CallingContextTree.Type.TopDown);
        this._bottomUpCallingContextTree = new WebInspector.CallingContextTree(WebInspector.CallingContextTree.Type.BottomUp);

        let timelines = [
            WebInspector.TimelineRecord.Type.Network,
            WebInspector.TimelineRecord.Type.Layout,
            WebInspector.TimelineRecord.Type.Script,
            WebInspector.TimelineRecord.Type.RenderingFrame,
            WebInspector.TimelineRecord.Type.Memory,
            WebInspector.TimelineRecord.Type.HeapAllocations,
        ];

        for (let type of timelines) {
            let timeline = WebInspector.Timeline.create(type);
            this._timelines.set(type, timeline);
            timeline.addEventListener(WebInspector.Timeline.Event.TimesUpdated, this._timelineTimesUpdated, this);
        }

        // For legacy backends, we compute the elapsed time of records relative to this timestamp.
        this._legacyFirstRecordedTimestamp = NaN;

        this.reset(true);
    }

    // Static

    static sourceCodeTimelinesSupported()
    {
        return WebInspector.debuggableType === WebInspector.DebuggableType.Web;
    }

    // Public

    get displayName()
    {
        return this._displayName;
    }

    get identifier()
    {
        return this._identifier;
    }

    get timelines()
    {
        return this._timelines;
    }

    get instruments()
    {
        return this._instruments;
    }

    get readonly()
    {
        return this._readonly;
    }

    get startTime()
    {
        return this._startTime;
    }

    get endTime()
    {
        return this._endTime;
    }

    get topDownCallingContextTree()
    {
        return this._topDownCallingContextTree;
    }

    get bottomUpCallingContextTree()
    {
        return this._bottomUpCallingContextTree;
    }

    start()
    {
        console.assert(!this._capturing, "Attempted to start an already started session.");
        console.assert(!this._readonly, "Attempted to start a readonly session.");

        this._capturing = true;

        for (let instrument of this._instruments)
            instrument.startInstrumentation();
    }

    stop()
    {
        console.assert(this._capturing, "Attempted to stop an already stopped session.");
        console.assert(!this._readonly, "Attempted to stop a readonly session.");

        this._capturing = false;

        for (let instrument of this._instruments)
            instrument.stopInstrumentation();
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

        this.dispatchEventToListeners(WebInspector.TimelineRecording.Event.Unloaded);
    }

    reset(suppressEvents)
    {
        console.assert(!this._readonly, "Can't reset a read-only recording.");

        this._sourceCodeTimelinesMap = new Map;
        this._eventMarkers = [];
        this._startTime = NaN;
        this._endTime = NaN;

        this._topDownCallingContextTree.reset();
        this._bottomUpCallingContextTree.reset();

        for (var timeline of this._timelines.values())
            timeline.reset(suppressEvents);

        WebInspector.RenderingFrameTimelineRecord.resetFrameIndex();

        if (!suppressEvents) {
            this.dispatchEventToListeners(WebInspector.TimelineRecording.Event.Reset);
            this.dispatchEventToListeners(WebInspector.TimelineRecording.Event.TimesUpdated);
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

    timelineForRecordType(recordType)
    {
        return this._timelines.get(recordType);
    }

    addInstrument(instrument)
    {
        console.assert(instrument instanceof WebInspector.Instrument, instrument);
        console.assert(!this._instruments.contains(instrument), this._instruments, instrument);

        this._instruments.push(instrument);

        this.dispatchEventToListeners(WebInspector.TimelineRecording.Event.InstrumentAdded, {instrument});
    }

    removeInstrument(instrument)
    {
        console.assert(instrument instanceof WebInspector.Instrument, instrument);
        console.assert(this._instruments.contains(instrument), this._instruments, instrument);

        this._instruments.remove(instrument);

        this.dispatchEventToListeners(WebInspector.TimelineRecording.Event.InstrumentRemoved, {instrument});
    }

    addEventMarker(marker)
    {
        if (!this._capturing)
            return;

        this._eventMarkers.push(marker);

        this.dispatchEventToListeners(WebInspector.TimelineRecording.Event.MarkerAdded, {marker});
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
        if (record.type === WebInspector.TimelineRecord.Type.Network
            || record.type === WebInspector.TimelineRecord.Type.RenderingFrame
            || record.type === WebInspector.TimelineRecord.Type.Memory
            || record.type === WebInspector.TimelineRecord.Type.HeapAllocations)
            return;

        if (!WebInspector.TimelineRecording.sourceCodeTimelinesSupported())
            return;

        // Add the record to the source code timelines.
        var activeMainResource = WebInspector.frameResourceManager.mainFrame.provisionalMainResource || WebInspector.frameResourceManager.mainFrame.mainResource;
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
            sourceCodeTimeline = new WebInspector.SourceCodeTimeline(sourceCode, record.sourceCodeLocation, record.type, record.eventType);
            sourceCodeTimelines.set(key, sourceCodeTimeline);
            newTimeline = true;
        }

        sourceCodeTimeline.addRecord(record);

        if (newTimeline)
            this.dispatchEventToListeners(WebInspector.TimelineRecording.Event.SourceCodeTimelineAdded, {sourceCodeTimeline});
    }

    addMemoryPressureEvent(memoryPressureEvent)
    {
        let memoryTimeline = this._timelines.get(WebInspector.TimelineRecord.Type.Memory);
        console.assert(memoryTimeline, this._timelines);
        if (!memoryTimeline)
            return;

        memoryTimeline.addMemoryPressureEvent(memoryPressureEvent);
    }

    computeElapsedTime(timestamp)
    {
        if (!timestamp || isNaN(timestamp))
            return NaN;

        // COMPATIBILITY (iOS 8): old backends send timestamps (seconds or milliseconds since the epoch),
        // rather than seconds elapsed since timeline capturing started. We approximate the latter by
        // subtracting the start timestamp, as old versions did not use monotonic times.
        if (WebInspector.TimelineRecording.isLegacy === undefined)
            WebInspector.TimelineRecording.isLegacy = timestamp > WebInspector.TimelineRecording.TimestampThresholdForLegacyRecordConversion;

        if (!WebInspector.TimelineRecording.isLegacy)
            return timestamp;

        // If the record's start time is large, but not really large, then it is seconds since epoch
        // not millseconds since epoch, so convert it to milliseconds.
        if (timestamp < WebInspector.TimelineRecording.TimestampThresholdForLegacyAssumedMilliseconds)
            timestamp *= 1000;

        if (isNaN(this._legacyFirstRecordedTimestamp))
            this._legacyFirstRecordedTimestamp = timestamp;

        // Return seconds since the first recorded value.
        return (timestamp - this._legacyFirstRecordedTimestamp) / 1000.0;
    }

    setLegacyBaseTimestamp(timestamp)
    {
        console.assert(isNaN(this._legacyFirstRecordedTimestamp));

        if (timestamp < WebInspector.TimelineRecording.TimestampThresholdForLegacyAssumedMilliseconds)
            timestamp *= 1000;

        this._legacyFirstRecordedTimestamp = timestamp;
    }

    initializeTimeBoundsIfNecessary(timestamp)
    {
        if (isNaN(this._startTime)) {
            console.assert(isNaN(this._endTime));

            this._startTime = timestamp;
            this._endTime = timestamp;

            this.dispatchEventToListeners(WebInspector.TimelineRecording.Event.TimesUpdated);
        }
    }

    // Private

    _keyForRecord(record)
    {
        var key = record.type;
        if (record instanceof WebInspector.ScriptTimelineRecord || record instanceof WebInspector.LayoutTimelineRecord)
            key += ":" + record.eventType;
        if (record instanceof WebInspector.ScriptTimelineRecord && record.eventType === WebInspector.ScriptTimelineRecord.EventType.EventDispatched)
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
            this.dispatchEventToListeners(WebInspector.TimelineRecording.Event.TimesUpdated);
    }
};

WebInspector.TimelineRecording.Event = {
    Reset: "timeline-recording-reset",
    Unloaded: "timeline-recording-unloaded",
    SourceCodeTimelineAdded: "timeline-recording-source-code-timeline-added",
    InstrumentAdded: "timeline-recording-instrument-added",
    InstrumentRemoved: "timeline-recording-instrument-removed",
    TimesUpdated: "timeline-recording-times-updated",
    MarkerAdded: "timeline-recording-marker-added",
};

WebInspector.TimelineRecording.isLegacy = undefined;
WebInspector.TimelineRecording.TimestampThresholdForLegacyRecordConversion = 10000000; // Some value not near zero.
WebInspector.TimelineRecording.TimestampThresholdForLegacyAssumedMilliseconds = 1420099200000; // Date.parse("Jan 1, 2015"). Milliseconds since epoch.
