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

WI.HeapAllocationsTimelineDataGridNode = class HeapAllocationsTimelineDataGridNode extends WI.TimelineDataGridNode
{
    constructor(heapAllocationsTimelineRecord, zeroTime, heapAllocationsView)
    {
        super(false, null);

        this._record = heapAllocationsTimelineRecord;
        this._heapAllocationsView = heapAllocationsView;

        this._data = {
            name: this.displayName(),
            timestamp: zeroTime ? this._record.timestamp - zeroTime : NaN,
            size: this._record.heapSnapshot.totalSize,
            liveSize: this._record.heapSnapshot.liveSize,
        };

        this._record.heapSnapshot.addEventListener(WI.HeapSnapshotProxy.Event.CollectedNodes, this._heapSnapshotCollectedNodes, this);
        this._record.heapSnapshot.addEventListener(WI.HeapSnapshotProxy.Event.Invalidated, this._heapSnapshotInvalidated, this);
    }

    // Public

    get record() { return this._record; }
    get data() { return this._data; }

    createCellContent(columnIdentifier, cell)
    {
        switch (columnIdentifier) {
        case "name":
            cell.classList.add(...this.iconClassNames());

            var fragment = document.createDocumentFragment();
            var titleElement = fragment.appendChild(document.createElement("span"));
            titleElement.textContent = this._data.name;
            if (!this._record.heapSnapshot.invalid) {
                var goToButton = fragment.appendChild(WI.createGoToArrowButton());
                goToButton.addEventListener("click", (event) => {
                    this._heapAllocationsView.showHeapSnapshotTimelineRecord(this._record);
                });
            }
            return fragment;

        case "timestamp":
            return isNaN(this._data.timestamp) ? emDash : Number.secondsToString(this._data.timestamp, true);

        case "size":
            return Number.bytesToString(this._data.size);

        case "liveSize":
            return Number.bytesToString(this._data.liveSize);
        }

        return super.createCellContent(columnIdentifier, cell);
    }

    markAsBaseline()
    {
        this.element.classList.add("baseline");
    }

    clearBaseline()
    {
        this.element.classList.remove("baseline");
    }

    updateTimestamp(zeroTime)
    {
        console.assert(isNaN(this._data.timestamp));
        this._data.timestamp = this._record.timestamp - zeroTime;
        this.needsRefresh();
    }

    // Protected

    createCells()
    {
        super.createCells();

        if (this._record.heapSnapshot.invalid)
            this.element.classList.add("invalid");
    }

    // Private

    _heapSnapshotCollectedNodes()
    {
        let oldSize = this._data.liveSize;
        let newSize = this._record.heapSnapshot.liveSize;

        console.assert(newSize <= oldSize);
        if (oldSize === newSize)
            return;

        this._data.liveSize = newSize;
        this.needsRefresh();
    }

    _heapSnapshotInvalidated()
    {
        this._data.liveSize = 0;

        this.needsRefresh();
    }
};
