/*
 * Copyright (C) 2018 Apple Inc. All rights reserved.
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

WI.MediaTimelineDataGridNode = class MediaTimelineDataGridNode extends WI.TimelineDataGridNode
{
    constructor(record, graphDataSource)
    {
        console.assert(record instanceof WI.MediaTimelineRecord);

        const includesGraph = false;
        super(includesGraph, graphDataSource);

        this._records = [record];
    }

    // Public

    get records() { return this._records; }

    get data()
    {
        if (this._cachedData)
            return this._cachedData;

        this._cachedData = super.data;
        this._cachedData.name = this.record.displayName;
        if (this.record.domNode)
            this._cachedData.element = this.record.domNode;
        this._cachedData.time = this.record.startTime - (this.graphDataSource ? this.graphDataSource.zeroTime : 0);
        if (this.record.eventType === WI.MediaTimelineRecord.EventType.DOMEvent && this.record.domEvent.originator)
            this._cachedData.originator = this.record.domEvent.originator;
        return this._cachedData;
    }

    createCellContent(columnIdentifier, cell)
    {
        let value = this.data[columnIdentifier];

        switch (columnIdentifier) {
        case "name":
            cell.classList.add(...this.iconClassNames());
            return value;

        case "element":
            return value ? WI.linkifyNodeReference(value) : emDash;

        case "time": {
            const higherResolution = true;
            return Number.secondsToString(value, higherResolution);
        }

        case "originator":
            return value ? WI.linkifyNodeReference(value) : zeroWidthSpace;
        }

        return super.createCellContent(columnIdentifier, cell);
    }

    iconClassNames()
    {
        let iconClassNames = super.iconClassNames();
        if (this.record.eventType === WI.MediaTimelineRecord.EventType.DOMEvent && this.record.domEvent.eventName === "webkitfullscreenchange")
            iconClassNames.push("fullscreen");
        return iconClassNames;
    }

    // Protected

    filterableDataForColumn(columnIdentifier)
    {
        if (columnIdentifier === "element") {
            if (this.record.domNode)
                return this.record.domNode.displayName;
        }

        if (columnIdentifier === "originator") {
            if (this.record.eventType === WI.MediaTimelineRecord.EventType.DOMEvent && this.record.domEvent.originator)
                return this.record.domEvent.originator.displayName;
        }

        return super.filterableDataForColumn(columnIdentifier);
    }
};
