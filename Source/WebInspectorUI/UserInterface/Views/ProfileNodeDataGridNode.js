/*
 * Copyright (C) 2014, 2015 Apple Inc. All rights reserved.
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

WebInspector.ProfileNodeDataGridNode = class ProfileNodeDataGridNode extends WebInspector.TimelineDataGridNode
{
    constructor(profileNode, baseStartTime, rangeStartTime, rangeEndTime)
    {
        var hasChildren = !!profileNode.childNodes.length;

        super(false, null, hasChildren);

        this._profileNode = profileNode;
        this._baseStartTime = baseStartTime || 0;
        this._rangeStartTime = rangeStartTime || 0;
        this._rangeEndTime = typeof rangeEndTime === "number" ? rangeEndTime : Infinity;

        this._cachedData = null;

        this.addEventListener("populate", this._populate, this);
    }

    // Public

    get profileNode()
    {
        return this._profileNode;
    }

    get records()
    {
        return null;
    }

    get baseStartTime()
    {
        return this._baseStartTime;
    }

    get rangeStartTime()
    {
        return this._rangeStartTime;
    }

    get rangeEndTime()
    {
        return this._rangeEndTime;
    }

    get data()
    {
        if (!this._cachedData) {
            this._cachedData = this._profileNode.computeCallInfoForTimeRange(this._rangeStartTime, this._rangeEndTime);
            this._cachedData.name = this.displayName();
            this._cachedData.location = this._profileNode.sourceCodeLocation;
        }

        return this._cachedData;
    }

    updateRangeTimes(startTime, endTime)
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
        var oldStartBoundary = Number.constrain(oldRangeStartTime, profileStart, profileEnd);
        var oldEndBoundary = Number.constrain(oldRangeEndTime, profileStart, profileEnd);
        var newStartBoundary = Number.constrain(startTime, profileStart, profileEnd);
        var newEndBoundary = Number.constrain(endTime, profileStart, profileEnd);

        if (oldStartBoundary !== newStartBoundary || oldEndBoundary !== newEndBoundary)
            this.needsRefresh();
    }

    refresh()
    {
        this._data = this._profileNode.computeCallInfoForTimeRange(this._rangeStartTime, this._rangeEndTime);
        this._data.location = this._profileNode.sourceCodeLocation;

        super.refresh();
    }

    createCellContent(columnIdentifier, cell)
    {
        var value = this.data[columnIdentifier];

        switch (columnIdentifier) {
        case "name":
            cell.classList.add(...this.iconClassNames());
            return value;

        case "startTime":
            return isNaN(value) ? emDash : Number.secondsToString(value - this._baseStartTime, true);

        case "selfTime":
        case "totalTime":
        case "averageTime":
            return isNaN(value) ? emDash : Number.secondsToString(value, true);
        }

        return super.createCellContent(columnIdentifier, cell);
    }

    displayName()
    {
        let title = this._profileNode.functionName;
        if (!title) {
            switch (this._profileNode.type) {
            case WebInspector.ProfileNode.Type.Function:
                title = WebInspector.UIString("(anonymous function)");
                break;
            case WebInspector.ProfileNode.Type.Program:
                title = WebInspector.UIString("(program)");
                break;
            default:
                title = WebInspector.UIString("(anonymous function)");
                console.error("Unknown ProfileNode type: " + this._profileNode.type);
            }
        }

        return title;
    }

    iconClassNames()
    {
        let className;
        switch (this._profileNode.type) {
        case WebInspector.ProfileNode.Type.Function:
            className = WebInspector.CallFrameView.FunctionIconStyleClassName;
            if (!this._profileNode.sourceCodeLocation)
                className = WebInspector.CallFrameView.NativeIconStyleClassName;
            break;
        case WebInspector.ProfileNode.Type.Program:
            className = WebInspector.TimelineRecordTreeElement.EvaluatedRecordIconStyleClass;
            break;
        }

        console.assert(className);

        // This is more than likely an event listener function with an "on" prefix and it is
        // as long or longer than the shortest event listener name -- "oncut".
        if (this._profileNode.functionName && this._profileNode.functionName.startsWith("on") && this._profileNode.functionName.length >= 5)
            className = WebInspector.CallFrameView.EventListenerIconStyleClassName;

        return [className];
    }

    // Private

    _populate()
    {
        if (!this.shouldRefreshChildren)
            return;

        this.removeEventListener("populate", this._populate, this);
        this.removeChildren();

        for (let node of this._profileNode.childNodes)
            this.appendChild(new WebInspector.ProfileNodeDataGridNode(node, this.baseStartTime, this.rangeStartTime, this.rangeEndTime));
    }
};
