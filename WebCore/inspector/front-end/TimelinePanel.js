/*
 * Copyright (C) 2009 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

WebInspector.TimelinePanel = function()
{
    WebInspector.Panel.call(this);

    this.createSidebar();

    this.element.addStyleClass("timeline");

    this.timelineView = document.createElement("div");
    this.timelineView.id = "timeline-view";
    this.element.appendChild(this.timelineView);

    this.recordsTreeElement = new WebInspector.SidebarSectionTreeElement(WebInspector.UIString("RECORDS"), {}, true);
    this.recordsTreeElement.expanded = true;
    this.sidebarTree.appendChild(this.recordsTreeElement);

    this.toggleTimelineButton = new WebInspector.StatusBarButton("", "record-profile-status-bar-item");
    this.toggleTimelineButton.addEventListener("click", this._toggleTimelineButton.bind(this), false);
}

WebInspector.TimelinePanel.prototype = {
    toolbarItemClass: "timeline",

    get toolbarItemLabel()
    {
        return WebInspector.UIString("Timeline");
    },

    get statusBarItems()
    {
        return [this.toggleTimelineButton.element];
    },

    timelineWasStarted: function()
    {
        this.toggleTimelineButton.toggled = true;
    },

    timelineWasStopped: function()
    {
        this.toggleTimelineButton.toggled = false;
    },

    addRecordToTimeline: function(record)
    {
        this._innerAddRecordToTimeline(this.recordsTreeElement, record);
    },

    _innerAddRecordToTimeline: function(parentElement, record)
    {
        var treeItem = new WebInspector.TimelineRecordTreeElement(this, record);
        parentElement.appendChild(treeItem);
        if (record.children)
            parentElement.expanded = true;
        for (var i = 0; i < record.children.length; ++i)
            this._innerAddRecordToTimeline(treeItem, record.children[i]);
    },

    _toggleTimelineButton: function()
    {
        if (InspectorController.timelineProfilerEnabled())
            InspectorController.stopTimelineProfiler();
        else
            InspectorController.startTimelineProfiler();
    },

    updateMainViewWidth: function(width)
    {
        this.timelineView.style.left = width + "px";
    },

    getRecordTypeName: function(record)
    {
        if (!this._recordTypeNames) {
            this._recordTypeNames = {};
            var recordTypes = WebInspector.TimelineAgent.RecordType;
            this._recordTypeNames[recordTypes.DOMDispatch] = WebInspector.UIString("DOM Event");
            this._recordTypeNames[recordTypes.Layout] = WebInspector.UIString("Layout");
            this._recordTypeNames[recordTypes.RecalculateStyles] = WebInspector.UIString("Recalculate Style");
            this._recordTypeNames[recordTypes.Paint] = WebInspector.UIString("Paint");
            this._recordTypeNames[recordTypes.Layout] = WebInspector.UIString("Layout");
            this._recordTypeNames[recordTypes.ParseHTML] = WebInspector.UIString("Parse");
        }
        return this._itemTypeNames[record.type];
    }
}

WebInspector.TimelinePanel.prototype.__proto__ = WebInspector.Panel.prototype;


WebInspector.TimelineRecordTreeElement = function(panel, record)
{
    this._panel = panel;
    this._record = record;

    // Pass an empty title, the title gets made later in onattach.
    TreeElement.call(this, "", null, false);
}

WebInspector.TimelineRecordTreeElement.prototype = {
    onattach: function()
    {
        this.listItemElement.removeChildren();
        this.listItemElement.addStyleClass("timeline-tree-item");

        var typeElement = document.createElement("span");
        typeElement.className = "type";
        typeElement.textContent = this._panel.getRecordTypeName(this._record);
        this.listItemElement.appendChild(typeElement);

        if (this._record.data) {
            var separatorElement = document.createElement("span");
            separatorElement.className = "separator";
            separatorElement.textContent = " ";

            var dataElement = document.createElement("span");
            dataElement.className = "data";
            dataElement.textContent = "(" + this._record.data.type + ")";
            dataElement.addStyleClass("dimmed");
            this.listItemElement.appendChild(separatorElement);
            this.listItemElement.appendChild(dataElement);
        }
    }
}

WebInspector.TimelineRecordTreeElement.prototype.__proto__ = TreeElement.prototype;
