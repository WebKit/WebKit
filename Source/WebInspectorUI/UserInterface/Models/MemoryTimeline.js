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

WI.MemoryTimeline = class MemoryTimeline extends WI.ProcessSampleTimeline
{
    // Public

    get memoryPressureEvents() { return this._pressureEvents; }

    addMemoryPressureEvent(memoryPressureEvent)
    {
        console.assert(memoryPressureEvent instanceof WI.MemoryPressureEvent);

        this._pressureEvents.push(memoryPressureEvent);

        this.dispatchEventToListeners(WI.MemoryTimeline.Event.MemoryPressureEventAdded, {memoryPressureEvent});
    }

    // Protected

    reset(suppressEvents)
    {
        super.reset(suppressEvents);

        this._pressureEvents = [];
    }

    addRecord(record, options = {})
    {
        let lastRecord = this.records.lastValue;
        if (lastRecord) {
            let startTime = lastRecord.endTime;
            if (options.discontinuity)
                startTime = options.discontinuity.endTime;
            record.adjustStartTime(startTime);
        }

        super.addRecord(record, options);
    }

    createAggregateRecord(startTime, endTime, recordsForProcess, liveProcessKeys)
    {
        // Memory is a level, so add up the latest sample of each process. A process that has stopped reporting keeps
        // its last value while it still has a target, so that a change of reporter does not dip the graph.
        let latestRecords = [];
        for (let [processKey, records] of recordsForProcess) {
            let index = records.upperBound(endTime, (time, record) => time - record.timestamp) - 1;
            if (index < 0)
                continue;

            let record = records[index];
            if (index === records.length - 1 && record.timestamp < endTime && !liveProcessKeys.has(processKey))
                continue;

            latestRecords.push(record);
        }

        // A single sample at the end of the bin, as always with one process.
        if (latestRecords.length === 1 && latestRecords[0].timestamp === endTime) {
            let {timestamp, protocolCategories} = latestRecords[0];
            return new WI.MemoryTimelineRecord(timestamp, protocolCategories);
        }

        // Add up the protocol categories, so that the record is exported like a sample.
        let sizeForType = new Map;
        for (let record of latestRecords) {
            for (let {type, size} of record.protocolCategories)
                sizeForType.set(type, (sizeForType.get(type) || 0) + size);
        }
        return new WI.MemoryTimelineRecord(endTime, Array.from(sizeForType, ([type, size]) => ({type, size})));
    }
};

WI.MemoryTimeline.Event = {
    MemoryPressureEventAdded: "memory-timeline-memory-pressure-event-added",
};
