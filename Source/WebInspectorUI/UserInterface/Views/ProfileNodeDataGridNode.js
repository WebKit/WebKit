/*
 * Copyright (C) 2014 Apple Inc. All rights reserved.
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

WebInspector.ProfileNodeDataGridNode = function(profileNode, baseStartTime, rangeStartTime, rangeEndTime)
{
    var hasChildren = !!profileNode.childNodes.length;

    WebInspector.TimelineDataGridNode.call(this, false, null, hasChildren);

    this._profileNode = profileNode;
    this._baseStartTime = baseStartTime || 0;
    this._rangeStartTime = rangeStartTime || 0;
    this._rangeEndTime = typeof rangeEndTime === "number" ? rangeEndTime : Infinity;

    this._data = this._profileNode.computeCallInfoForTimeRange(this._rangeStartTime, this._rangeEndTime);
    this._data.location = this._profileNode.sourceCodeLocation;
};

WebInspector.Object.addConstructorFunctions(WebInspector.ProfileNodeDataGridNode);

WebInspector.ProfileNodeDataGridNode.IconStyleClassName = "icon";

WebInspector.ProfileNodeDataGridNode.prototype = {
    constructor: WebInspector.ProfileNodeDataGridNode,
    __proto__: WebInspector.TimelineDataGridNode.prototype,

    // Public

    get profileNode()
    {
        return this._profileNode;
    },

    get records()
    {
        return null;
    },

    get baseStartTime()
    {
        return this._baseStartTime;
    },

    get rangeStartTime()
    {
        return this._rangeStartTime;
    },

    get rangeEndTime()
    {
        return this._rangeEndTime;
    },

    get data()
    {
        return this._data;
    },

    updateRangeTimes: function(startTime, endTime)
    {
        var oldRangeStartTime = this._rangeStartTime;
        var oldRangeEndTime = this._rangeEndTime;

        if (oldRangeStartTime === startTime && oldRangeEndTime === endTime)
            return;

        this._rangeStartTime = startTime;
        this._rangeEndTime = endTime;

        // We only need a refresh if the new range time changes the visible portion of this record.
        var profileStart = this._profileNode.startTime;
        var profileEnd = this._profileNode.endTime;
        var oldStartBoundary = clamp(profileStart, oldRangeStartTime, profileEnd);
        var oldEndBoundary = clamp(profileStart, oldRangeEndTime, profileEnd);
        var newStartBoundary = clamp(profileStart, startTime, profileEnd);
        var newEndBoundary = clamp(profileStart, endTime, profileEnd);

        if (oldStartBoundary !== newStartBoundary || oldEndBoundary !== newEndBoundary)
            this.needsRefresh();
    },

    refresh: function()
    {
        this._data = this._profileNode.computeCallInfoForTimeRange(this._rangeStartTime, this._rangeEndTime);
        this._data.location = this._profileNode.sourceCodeLocation;

        WebInspector.TimelineDataGridNode.prototype.refresh.call(this);
    },

    createCellContent: function(columnIdentifier, cell)
    {
        const emptyValuePlaceholderString = "\u2014";
        var value = this.data[columnIdentifier];

        switch (columnIdentifier) {
        case "startTime":
            return isNaN(value) ? emptyValuePlaceholderString : Number.secondsToString(value - this._baseStartTime, true);

        case "selfTime":
        case "totalTime":
        case "averageTime":
            return isNaN(value) ? emptyValuePlaceholderString : Number.secondsToString(value, true);
        }

        return WebInspector.TimelineDataGridNode.prototype.createCellContent.call(this, columnIdentifier, cell);
    }
};
