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

WebInspector.HeapAllocationsTimelineDataGridNode = class HeapAllocationsTimelineDataGridNode extends WebInspector.TimelineDataGridNode
{
    constructor(heapAllocationsTimelineRecord, zeroTime, heapAllocationsView)
    {
        super(false, null);

        this._record = heapAllocationsTimelineRecord;
        this._heapAllocationsView = heapAllocationsView;

        this._data = {
            name: WebInspector.TimelineTabContentView.displayNameForRecord(heapAllocationsTimelineRecord),
            timestamp: this._record.timestamp - zeroTime,
            size: this._record.heapSnapshot.totalSize,
        };
    }

    // Public

    get record() { return this._record; }
    get data() { return this._data; }

    createCellContent(columnIdentifier, cell)
    {
        switch (columnIdentifier) {
        case "name":
            let fragment = document.createDocumentFragment();
            let iconElement = fragment.appendChild(document.createElement("img"));
            iconElement.classList.add("icon", "heap-snapshot");
            let titleElement = fragment.appendChild(document.createElement("span"));
            titleElement.textContent = this._data.name;
            let goToButton = fragment.appendChild(WebInspector.createGoToArrowButton());
            goToButton.addEventListener("click", (event) => {
                this._heapAllocationsView.showHeapSnapshotTimelineRecord(this._record);
            });
            return fragment;

        case "timestamp":
            return Number.secondsToString(this._data.timestamp, true);

        case "size":
            return Number.bytesToString(this._data.size);
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
};
