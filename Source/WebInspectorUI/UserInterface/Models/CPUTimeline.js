/*
 * Copyright (C) 2019 Apple Inc. All rights reserved.
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

WI.CPUTimeline = class CPUTimeline extends WI.ProcessSampleTimeline
{
    // Public

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

    // Protected

    createAggregateRecord(startTime, endTime, recordsForProcess, liveProcessKeys)
    {
        const samplingInterval = WI.CPUTimelineRecord.samplingRatePerSecond;
        if (isNaN(startTime))
            startTime = endTime - samplingInterval;

        // Usage is a rate, and processes sample at different times. A sample covers the time since the previous sample
        // of its process, so weigh it by how much of the bin it covers.
        let duration = endTime - startTime;
        let weightedRecordsForProcess = new Map;
        for (let [processKey, records] of recordsForProcess) {
            let weightedRecords = [];
            for (let i = records.upperBound(startTime, (time, record) => time - record.timestamp); i < records.length; ++i) {
                let record = records[i];
                let recordStartTime = i ? records[i - 1].timestamp : record.timestamp - samplingInterval;
                if (recordStartTime >= endTime)
                    break;

                let weight = (Math.min(record.timestamp, endTime) - Math.max(recordStartTime, startTime)) / duration;
                weightedRecords.push({record, weight});
            }
            if (weightedRecords.length)
                weightedRecordsForProcess.set(processKey, weightedRecords);
        }

        // A single sample covers the bin, as always with one process.
        if (weightedRecordsForProcess.size === 1) {
            let [weightedRecords] = weightedRecordsForProcess.values();
            if (weightedRecords.length === 1 && weightedRecords[0].weight === 1) {
                let {timestamp, usage, threads} = weightedRecords[0].record;
                return new WI.CPUTimelineRecord({timestamp, usage, threads});
            }
        }

        let usage = 0;
        let threads = [];
        for (let [processKey, weightedRecords] of weightedRecordsForProcess) {
            let threadForKey = new Map;
            for (let {record, weight} of weightedRecords) {
                usage += record.usage * weight;

                // Threads have no identifier, so match them across samples by type, name and order.
                let countForKey = new Map;
                for (let thread of record.threads) {
                    let key = thread.type + ":" + (thread.targetId || thread.name);
                    let count = countForKey.get(key) || 0;
                    countForKey.set(key, count + 1);

                    let aggregateThread = threadForKey.getOrInsertComputed(key + ":" + count, () => ({...thread, usage: 0, processKey}));
                    aggregateThread.usage += thread.usage * weight;
                }
            }
            threads.pushAll(threadForKey.values());
        }

        return new WI.CPUTimelineRecord({timestamp: endTime, usage, threads});
    }
};
