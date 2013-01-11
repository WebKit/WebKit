/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
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

/**
 * @constructor
 * @extends {WebInspector.View}
 */
WebInspector.CanvasProfileView = function(profile)
{
    WebInspector.View.call(this);
    this.registerRequiredCSS("canvasProfiler.css");
    this._profile = profile;
    this._traceLogId = profile.traceLogId();
    this.element.addStyleClass("canvas-profile-view");

    this._linkifier = new WebInspector.Linkifier();
    this._splitView = new WebInspector.SplitView(false, "canvasProfileViewSplitLocation", 300);

    var columns = { 0: {}, 1: {}, 2: {} };
    columns[0].title = "#";
    columns[0].sortable = true;
    columns[0].width = "5%";
    columns[1].title = WebInspector.UIString("Call");
    columns[1].sortable = true;
    columns[1].width = "75%";
    columns[2].title = WebInspector.UIString("Location");
    columns[2].sortable = true;
    columns[2].width = "20%";

    this._logGrid = new WebInspector.DataGrid(columns);
    this._logGrid.element.addStyleClass("fill");
    this._logGrid.show(this._splitView.secondElement());
    this._logGrid.addEventListener(WebInspector.DataGrid.Events.SelectedNode, this._replayTraceLog.bind(this));

    var replayImageContainer = this._splitView.firstElement();
    replayImageContainer.id = "canvas-replay-image-container";

    this._replayImageElement = document.createElement("image");
    this._replayImageElement.id = "canvas-replay-image";

    replayImageContainer.appendChild(this._replayImageElement);
    this._debugInfoElement = document.createElement("div");
    replayImageContainer.appendChild(this._debugInfoElement);

    this._splitView.show(this.element);

    this._enableWaitIcon(true);
    CanvasAgent.getTraceLog(this._traceLogId, 0, this._didReceiveTraceLog.bind(this));
}

WebInspector.CanvasProfileView.prototype = {
    dispose: function()
    {
        this._linkifier.reset();
        CanvasAgent.dropTraceLog(this._traceLogId);
    },

    get statusBarItems()
    {
        return [];
    },

    get profile()
    {
        return this._profile;
    },

    /**
     * @override
     * @return {Array.<Element>}
     */
    elementsToRestoreScrollPositionsFor: function()
    {
        return [this._logGrid.scrollContainer];
    },

    /**
     * @param {boolean} enable
     */
    _enableWaitIcon: function(enable)
    {
        function showWaitIcon()
        {
            this._replayImageElement.className = "wait";
            this._debugInfoElement.textContent = "";
            delete this._showWaitIconTimer;
        }

        if (enable && this._replayImageElement.src && !this._showWaitIconTimer)
            this._showWaitIconTimer = setTimeout(showWaitIcon.bind(this), 250);
        else {
            if (this._showWaitIconTimer) {
                clearTimeout(this._showWaitIconTimer);
                delete this._showWaitIconTimer;
            }
            this._replayImageElement.className = enable ? "wait" : "";
            this._debugInfoElement.textContent = "";
        }
    },

    _replayTraceLog: function()
    {
        var callNode = this._logGrid.selectedNode;
        if (!callNode)
            return;
        var time = Date.now();
        function didReplayTraceLog(error, dataURL)
        {
            this._enableWaitIcon(false);
            if (error)
                return;
            this._debugInfoElement.textContent = "Replay time: " + (Date.now() - time) + "ms";
            this._replayImageElement.src = dataURL;
        }
        this._enableWaitIcon(true);
        CanvasAgent.replayTraceLog(this._traceLogId, callNode.index, didReplayTraceLog.bind(this));
    },

    _didReceiveTraceLog: function(error, traceLog)
    {
        this._enableWaitIcon(false);
        this._logGrid.rootNode().removeChildren();
        if (error || !traceLog)
            return;
        var calls = traceLog.calls;
        for (var i = 0, n = calls.length; i < n; ++i)
            this._logGrid.rootNode().appendChild(this._createCallNode(i, calls[i]));
        var lastNode = this._logGrid.rootNode().children[calls.length - 1];
        if (lastNode) {
            lastNode.reveal();
            lastNode.select();
        }
    },

    _createCallNode: function(index, call)
    {
        var traceLogItem = document.createElement("div");
        var data = {};
        data[0] = index + 1;
        data[1] = call.functionName || "context." + call.property;
        data[2] = "";
        if (call.sourceURL) {
            // FIXME(62725): stack trace line/column numbers are one-based.
            var lineNumber = Math.max(0, call.lineNumber - 1) || 0;
            var columnNumber = Math.max(0, call.columnNumber - 1) || 0;
            data[2] = this._linkifier.linkifyLocation(call.sourceURL, lineNumber, columnNumber);
        }

        if (call.arguments)
            data[1] += "(" + call.arguments.join(", ") + ")";
        else
            data[1] += " = " + call.value;

        if (typeof call.result !== "undefined")
            data[1] += " => " + call.result;

        var node = new WebInspector.DataGridNode(data);
        node.call = call;
        node.index = index;
        node.selectable = true;
        return node;
    },

    __proto__: WebInspector.View.prototype
}

/**
 * @constructor
 * @extends {WebInspector.ProfileType}
 */
WebInspector.CanvasProfileType = function()
{
    WebInspector.ProfileType.call(this, WebInspector.CanvasProfileType.TypeId, WebInspector.UIString("Capture Canvas Frame"));
    this._nextProfileUid = 1;

    this._decorationElement = document.createElement("div");
    this._decorationElement.addStyleClass("profile-canvas-decoration");
    this._decorationElement.addStyleClass("hidden");
    this._decorationElement.textContent = WebInspector.UIString("There is an uninstrumented canvas on the page. Reload the page to instrument it.");
    var reloadPageButton = this._decorationElement.createChild("button");
    reloadPageButton.type = "button";
    reloadPageButton.textContent = WebInspector.UIString("Reload");
    reloadPageButton.addEventListener("click", this._onReloadPageButtonClick.bind(this), false);

    // FIXME: enable/disable by a UI action?
    CanvasAgent.enable(this._updateDecorationElement.bind(this));
    WebInspector.resourceTreeModel.addEventListener(WebInspector.ResourceTreeModel.EventTypes.MainFrameNavigated, this._updateDecorationElement, this);
}

WebInspector.CanvasProfileType.TypeId = "CANVAS_PROFILE";

WebInspector.CanvasProfileType.prototype = {
    get buttonTooltip()
    {
        return WebInspector.UIString("Capture Canvas Frame.");
    },

    /**
     * @override
     * @param {WebInspector.ProfilesPanel} profilesPanel
     * @return {boolean}
     */
    buttonClicked: function(profilesPanel)
    {
        var profileHeader = new WebInspector.CanvasProfileHeader(this, WebInspector.UIString("Trace Log %d", this._nextProfileUid), this._nextProfileUid);
        ++this._nextProfileUid;
        profileHeader.isTemporary = true;
        profilesPanel.addProfileHeader(profileHeader);
        function didStartCapturingFrame(error, traceLogId)
        {
            profileHeader._traceLogId = traceLogId;
            profileHeader.isTemporary = false;
        }
        CanvasAgent.captureFrame(didStartCapturingFrame.bind(this));
        return false;
    },

    get treeItemTitle()
    {
        return WebInspector.UIString("CANVAS PROFILE");
    },

    get description()
    {
        return WebInspector.UIString("Canvas calls instrumentation");
    },

    /**
     * @override
     * @return {Element}
     */
    decorationElement: function()
    {
        return this._decorationElement;
    },

    /**
     * @override
     */
    reset: function()
    {
        this._nextProfileUid = 1;
    },

    /**
     * @override
     * @param {string=} title
     * @return {WebInspector.ProfileHeader}
     */
    createTemporaryProfile: function(title)
    {
        title = title || WebInspector.UIString("Capturing\u2026");
        return new WebInspector.CanvasProfileHeader(this, title);
    },

    /**
     * @override
     * @param {ProfilerAgent.ProfileHeader} profile
     * @return {WebInspector.ProfileHeader}
     */
    createProfile: function(profile)
    {
        return new WebInspector.CanvasProfileHeader(this, profile.title, -1);
    },

    _updateDecorationElement: function()
    {
        function callback(error, result)
        {
            var showWarning = (!error && result);
            if (showWarning)
                this._decorationElement.removeStyleClass("hidden");
            else
                this._decorationElement.addStyleClass("hidden");
        }
        CanvasAgent.hasUninstrumentedCanvases(callback.bind(this));
    },

    /**
     * @param {MouseEvent} event
     */
    _onReloadPageButtonClick: function(event)
    {
        PageAgent.reload(event.shiftKey);
    },

    __proto__: WebInspector.ProfileType.prototype
}

/**
 * @constructor
 * @extends {WebInspector.ProfileHeader}
 * @param {!WebInspector.CanvasProfileType} type
 * @param {string} title
 * @param {number=} uid
 */
WebInspector.CanvasProfileHeader = function(type, title, uid)
{
    WebInspector.ProfileHeader.call(this, type, title, uid);

    /**
     * @type {string?}
     */
    this._traceLogId = null;
}

WebInspector.CanvasProfileHeader.prototype = {
    /**
     * @return {string?}
     */
    traceLogId: function()
    {
        return this._traceLogId;
    },

    /**
     * @override
     */
    createSidebarTreeElement: function()
    {
        return new WebInspector.ProfileSidebarTreeElement(this, WebInspector.UIString("Trace Log %d"), "profile-sidebar-tree-item");
    },

    /**
     * @override
     * @param {WebInspector.ProfilesPanel} profilesPanel
     */
    createView: function(profilesPanel)
    {
        return new WebInspector.CanvasProfileView(this);
    },

    __proto__: WebInspector.ProfileHeader.prototype
}
