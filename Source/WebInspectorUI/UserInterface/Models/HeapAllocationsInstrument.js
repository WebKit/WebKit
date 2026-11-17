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

WI.HeapAllocationsInstrument = class HeapAllocationsInstrument extends WI.Instrument
{
    constructor()
    {
        super();

        this._snapshotIntervalIdentifier = undefined;
        this._shouldStartTrackingForTargets = false;
        this._listeningForFrameCommits = false;
        this._committedFrameIdentifiers = new Set;
        this._targetsStartedAfterCommit = new Set;
    }

    // Protected

    get timelineRecordType()
    {
        return WI.TimelineRecord.Type.HeapAllocations;
    }

    startInstrumentation(initiatingTarget, {autoCapture} = {})
    {
        // FIXME: Include a "track allocations" option for this instrument.
        // FIXME: Include a periodic snapshot interval option for this instrument.

        // An auto-capture starts when the main frame starts loading. Starting Heap takes a snapshot, which runs a full
        // garbage collection, so don't start it on other targets while the page loads (a new frame's load even waits
        // until its target is set up). Instead, the target that started the auto-capture starts its own Heap once the
        // new document is committed, and each frame target is started once its frame has committed a document.
        this._shouldStartTrackingForTargets = !autoCapture;

        if (this._shouldStartTrackingForTargets) {
            for (let target of this.#allSupportedTargets()) {
                if (target !== initiatingTarget)
                    this.#startTrackingForTarget(target);
            }
        } else {
            this._listeningForFrameCommits = true;
            WI.Frame.addEventListener(WI.Frame.Event.MainResourceDidChange, this._handleFrameMainResourceDidChange, this);
            WI.FrameTarget.addEventListener(WI.FrameTarget.Event.ExecutionContextAdded, this._handleFrameTargetExecutionContextAdded, this);
        }

        // Periodic snapshots.
        const snapshotInterval = 10_000;
        this._snapshotIntervalIdentifier = setInterval(this._takeHeapSnapshot.bind(this), snapshotInterval);
    }

    startInstrumentationForTarget(target)
    {
        if (this._shouldStartTrackingForTargets && this.#isSupportedTarget(target))
            this.#startTrackingForTarget(target);
    }

    stopInstrumentation()
    {
        // TimelineManager stops every domain that is tracking.

        window.clearInterval(this._snapshotIntervalIdentifier);
        this._snapshotIntervalIdentifier = undefined;

        if (this._listeningForFrameCommits) {
            this._listeningForFrameCommits = false;
            WI.Frame.removeEventListener(WI.Frame.Event.MainResourceDidChange, this._handleFrameMainResourceDidChange, this);
            WI.FrameTarget.removeEventListener(WI.FrameTarget.Event.ExecutionContextAdded, this._handleFrameTargetExecutionContextAdded, this);
            this._committedFrameIdentifiers.clear();
            this._targetsStartedAfterCommit.clear();
        }
    }

    // Private

    _takeHeapSnapshot()
    {
        for (let target of this.#allSupportedTargets()) {
            WI.heapManager.snapshot(target, (error, timestamp, snapshotStringData) => {
                if (error)
                    return;

                let workerProxy = WI.HeapSnapshotWorkerProxy.singleton();
                workerProxy.createSnapshot(target.identifier, snapshotStringData, ({objectId, snapshot: serializedSnapshot}) => {
                    let snapshot = WI.HeapSnapshotProxy.deserialize(target, objectId, serializedSnapshot);
                    snapshot.snapshotStringData = snapshotStringData;
                    WI.timelineManager.heapSnapshotAdded(timestamp, snapshot);
                });
            });
        }
    }

    // A frame's commit and the frame target's execution context for the new document can arrive in either order: a
    // frame target that already exists keeps its old context (with the same frame identifier) until after the commit,
    // and a frame target created during the load reports its first context after the commit.
    _handleFrameMainResourceDidChange(event)
    {
        let frame = event.target;

        // A frame inserted by script commits its initial about:blank document before its real one, so wait for the real
        // one. A frame that stays on about:blank only gets periodic snapshots.
        if (/^about:blank([?#]|$)/.test(frame.mainResource.url))
            return;

        this._committedFrameIdentifiers.add(frame.id);

        for (let target of WI.targets) {
            if (target instanceof WI.FrameTarget && target.executionContext?.frameId === frame.id)
                this.#startTrackingAfterCommitForTarget(target);
        }
    }

    _handleFrameTargetExecutionContextAdded(event)
    {
        let {context} = event.data;
        if (context.type === WI.ExecutionContext.Type.Normal && this._committedFrameIdentifiers.has(context.frameId))
            this.#startTrackingAfterCommitForTarget(event.target);
    }

    #startTrackingAfterCommitForTarget(target)
    {
        if (target.isDestroyed || this._targetsStartedAfterCommit.has(target) || !this.#isSupportedTarget(target))
            return;

        this._targetsStartedAfterCommit.add(target);
        this.#startTrackingForTarget(target);
    }

    #startTrackingForTarget(target)
    {
        WI.timelineManager.willStartTrackingForTarget(target, "Heap");
        target.HeapAgent.startTracking((error) => {
            // A target that failed to start (e.g. the page target under Site Isolation, if it is asked before the frame
            // targets exist) never reports that it stopped, so stop waiting for it.
            if (error)
                WI.timelineManager.heapTrackingCompleted(target);
        });
    }

    #isSupportedTarget(target)
    {
        return WI.heapManager.targetReportsHeap(target);
    }

    *#allSupportedTargets() {
        for (let target of WI.targets) {
            if (this.#isSupportedTarget(target))
                yield target;
        }
    }
};
