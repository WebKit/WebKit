/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
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

// CPU and Memory samples describe a whole process, and the targets of one page can run in several processes. Keep the
// samples of each process, and make one record per bin that combines every process. Bins end at the samples of one
// process (the clock), so with a single process each record has the values of one sample.
WI.ProcessSampleTimeline = class ProcessSampleTimeline extends WI.Timeline
{
    // Static

    static processKeyForTarget(target)
    {
        return target.processIdentifier || target.identifier;
    }

    // Public

    get processRecords() { return this._processRecords; }

    addProcessRecord(target, record, liveTargets)
    {
        console.assert(record.type === this.type, record);

        let processKey = WI.ProcessSampleTimeline.processKeyForTarget(target);

        // When another target takes over reporting for a process, the previous one can still deliver a sample.
        let processRecords = this._processRecords.getOrInsertComputed(processKey, () => []);
        if (processRecords.length && record.timestamp <= processRecords.lastValue.timestamp)
            return [];

        processRecords.push(record);
        this._captureRecordsForProcess.getOrInsertComputed(processKey, () => []).push(record);
        this._reportingTargetForProcess.set(processKey, target);

        if (!this._clockProcessKey || !this._isReporting(this._clockProcessKey, liveTargets))
            this._clockProcessKey = processKey;

        let lastBinEndTime = this._pendingBinEndTimes.lastValue ?? this._lastBinEndTime;
        if (processKey === this._clockProcessKey && (isNaN(lastBinEndTime) || record.timestamp > lastBinEndTime))
            this._pendingBinEndTimes.push(record.timestamp);

        return this._takeAggregateRecords(liveTargets);
    }

    flushProcessRecords(liveTargets)
    {
        let aggregateRecords = this._takeAggregateRecords(liveTargets, {flush: true});
        this._resetCapture();
        return aggregateRecords;
    }

    // Protected

    reset(suppressEvents)
    {
        super.reset(suppressEvents);

        this._processRecords = new Map;
        this._resetCapture();
    }

    createAggregateRecord(startTime, endTime, recordsForProcess, liveProcessKeys)
    {
        // `startTime` is NaN for the first bin of a capture.
        throw WI.NotImplementedError.subclassMustOverride();
    }

    // Private

    _resetCapture()
    {
        this._captureRecordsForProcess = new Map;
        this._reportingTargetForProcess = new Map;
        this._clockProcessKey = null;
        this._pendingBinEndTimes = [];
        this._lastBinEndTime = NaN;
    }

    // Only the target that last reported counts. Another target of the process may never report, e.g. a frame of a
    // new page in the old page's process.
    _isReporting(processKey, liveTargets)
    {
        return liveTargets.includes(this._reportingTargetForProcess.get(processKey));
    }

    _isWaitingForProcess(endTime, liveTargets)
    {
        for (let [processKey, records] of this._captureRecordsForProcess) {
            if (records.lastValue.timestamp < endTime && this._isReporting(processKey, liveTargets))
                return true;
        }
        return false;
    }

    _takeAggregateRecords(liveTargets, {flush} = {})
    {
        let liveProcessKeys = new Set(liveTargets.map(WI.ProcessSampleTimeline.processKeyForTarget));
        let aggregateRecords = [];
        while (this._pendingBinEndTimes.length) {
            let endTime = this._pendingBinEndTimes[0];

            // A process delivers its samples late while its main thread is busy. Wait for them while the target that
            // reports for it exists, so that they count.
            if (!flush && this._isWaitingForProcess(endTime, liveTargets))
                break;

            this._pendingBinEndTimes.shift();
            aggregateRecords.push(this.createAggregateRecord(this._lastBinEndTime, endTime, this._captureRecordsForProcess, liveProcessKeys));
            this._lastBinEndTime = endTime;
        }
        return aggregateRecords;
    }
};
