/*
 * Copyright (C) 2026 Devin Rousso <webkit@devinrousso.com>. All rights reserved.
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

WI.UserTimingTimelineDataGridNode = class UserTimingTimelineDataGridNode extends WI.TimelineDataGridNode
{
    constructor(record, options = {})
    {
        console.assert(record instanceof WI.UserTimingTimelineRecord, record);

        super([record], options);

        this._detailElement = null;

        if (record.updatesDynamically)
            record.addEventListener(WI.TimelineRecord.Event.Updated, this._handleRecordUpdated, this);
    }

    // Public

    get data()
    {
        if (this._cachedData)
            return this._cachedData;

        this._cachedData = super.data;
        this._cachedData.name = this.record.label;
        this._cachedData.startTime = this.record.startTime - (this.graphDataSource?.zeroTime || 0);
        this._cachedData.endTime = this.record.endTime - (this.graphDataSource?.zeroTime || 0);
        this._cachedData.totalTime = this.record.duration;
        this._cachedData.detail = this.record.detail;
        this._cachedData.location = this.record.initiatorCallFrame || this.record.sourceCodeLocation;
        this._cachedData.source = this._cachedData.location; // Timeline Overview.
        return this._cachedData;
    }

    refresh()
    {
        this._cachedData = null;

        super.refresh();
    }

    createCellContent(columnIdentifier, cell)
    {
        const higherResolution = true;

        switch (columnIdentifier) {
        case "name":
            cell.classList.add(...this.iconClassNames());
            return this.record.label || emDash;

        case "startTime":
        case "endTime":
        case "totalTime":
            return isNaN(this.data[columnIdentifier]) ? emDash : Number.secondsToString(this.data[columnIdentifier], higherResolution);

        case "detail": {
            let detail = this.record.detail;
            if (!detail)
                return emDash;
            if (!this._detailElement) {
                if (detail instanceof WI.RemoteObject) {
                    if (detail.hasChildren) {
                        let objectTree = new WI.ObjectTreeView(detail, WI.ObjectTreeView.Mode.Properties);
                        objectTree.hidePrototype();

                        this._detailElement = objectTree.element;
                    } else
                        this._detailElement = WI.FormattedValue.createObjectTreeOrFormattedValueForRemoteObject(detail);
                } else
                    this._detailElement = WI.FormattedValue.createObjectPreviewOrFormattedValueForObjectPreview(detail);
            }
            return this._detailElement;
        }

        case "source": // Timeline Overview
            return super.createCellContent("location", cell);
        }

        return super.createCellContent(columnIdentifier, cell);
    }

    // Private

    _handleRecordUpdated(event)
    {
        this.needsRefresh();
    }
};
