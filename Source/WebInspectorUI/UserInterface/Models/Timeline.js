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

WI.Timeline = class Timeline extends WI.Object
{
    constructor(type)
    {
        super();

        this._type = type;

        this.reset(true);
    }

    // Static

    static create(type)
    {
        if (type === WI.TimelineRecord.Type.Network)
            return new WI.NetworkTimeline(type);

        if (type === WI.TimelineRecord.Type.Memory)
            return new WI.MemoryTimeline(type);

        return new WI.Timeline(type);
    }

    // Public

    get type() { return this._type; }
    get startTime() { return this._startTime; }
    get endTime() { return this._endTime; }
    get records() { return this._records; }

    reset(suppressEvents)
    {
        this._records = [];
        this._startTime = NaN;
        this._endTime = NaN;

        if (!suppressEvents) {
            this.dispatchEventToListeners(WI.Timeline.Event.TimesUpdated);
            this.dispatchEventToListeners(WI.Timeline.Event.Reset);
        }
    }

    addRecord(record)
    {
        if (record.updatesDynamically)
            record.addEventListener(WI.TimelineRecord.Event.Updated, this._recordUpdated, this);

        // Because records can be nested, it is possible that outer records with an early start time
        // may be completed and added to the Timeline after inner records with a later start time
        // were already added. In most cases this is a small drift, so make an effort to still keep
        // the list sorted. Do it now, when inserting, so if the timeline is visible it has the
        // best chance of being as accurate as possible during a recording.
        this._tryInsertingRecordInSortedOrder(record);

        this._updateTimesIfNeeded(record);

        this.dispatchEventToListeners(WI.Timeline.Event.RecordAdded, {record});
    }

    saveIdentityToCookie(cookie)
    {
        cookie[WI.Timeline.TimelineTypeCookieKey] = this._type;
    }

    refresh()
    {
        this.dispatchEventToListeners(WI.Timeline.Event.Refreshed);
    }

    recordsOverlappingTimeRange(startTime, endTime)
    {
        let lowerIndex = this._records.lowerBound(startTime, (time, record) => time - record.endTime);
        let upperIndex = this._records.upperBound(endTime, (time, record) => time - record.startTime);

        return this._records.slice(lowerIndex, upperIndex);
    }

    recordsInTimeRange(startTime, endTime, includeRecordBeforeStart)
    {
        let lowerIndex = this._records.lowerBound(startTime, (time, record) => time - record.startTime);
        let upperIndex = this._records.upperBound(endTime, (time, record) => time - record.startTime);

        // Include the record right before the start time.
        if (includeRecordBeforeStart && lowerIndex > 0)
            lowerIndex--;

        return this._records.slice(lowerIndex, upperIndex);
    }

    // Private

    _updateTimesIfNeeded(record)
    {
        var changed = false;

        if (isNaN(this._startTime) || record.startTime < this._startTime) {
            this._startTime = record.startTime;
            changed = true;
        }

        if (isNaN(this._endTime) || this._endTime < record.endTime) {
            this._endTime = record.endTime;
            changed = true;
        }

        if (changed)
            this.dispatchEventToListeners(WI.Timeline.Event.TimesUpdated);
    }

    _recordUpdated(event)
    {
        this._updateTimesIfNeeded(event.target);
    }

    _tryInsertingRecordInSortedOrder(record)
    {
        // Fast case add to the end.
        let lastValue = this._records.lastValue;
        if (!lastValue || lastValue.startTime < record.startTime || record.updatesDynamically) {
            this._records.push(record);
            return;
        }

        // Slow case, try to insert in the last 20 records.
        let start = this._records.length - 2;
        let end = Math.max(this._records.length - 20, 0);
        for (let i = start; i >= end; --i) {
            if (this._records[i].startTime < record.startTime) {
                this._records.insertAtIndex(record, i + 1);
                return;
            }
        }

        // Give up and add to the end.
        this._records.push(record);
    }
};

WI.Timeline.Event = {
    Reset: "timeline-reset",
    RecordAdded: "timeline-record-added",
    TimesUpdated: "timeline-times-updated",
    Refreshed: "timeline-refreshed",
};

WI.Timeline.TimelineTypeCookieKey = "timeline-type";
