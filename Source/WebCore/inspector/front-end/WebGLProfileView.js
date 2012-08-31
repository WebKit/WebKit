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
WebInspector.WebGLProfileView = function(profile)
{
    WebInspector.View.call(this);
    this.registerRequiredCSS("webGLProfiler.css");
    this._profile = profile;
    this.element.addStyleClass("webgl-profile-view");

    this._traceLogElement = document.createElement("div");
    this._traceLogElement.className = "webgl-trace-log";
    this._traceLogElement.addEventListener("click", this._onTraceLogItemClick.bind(this), false);
    this.element.appendChild(this._traceLogElement);

    var replayImageContainer = document.createElement("div");
    replayImageContainer.id = "webgl-replay-image-container";
    this.element.appendChild(replayImageContainer);

    this._replayImageElement = document.createElement("image");
    this._replayImageElement.id = "webgl-replay-image";
    replayImageContainer.appendChild(this._replayImageElement);

    this._debugInfoElement = document.createElement("div");
    replayImageContainer.appendChild(this._debugInfoElement);

    this._linkifier = new WebInspector.Linkifier();

    this._showTraceLog();
}

WebInspector.WebGLProfileView.prototype = {
    dispose: function()
    {
        this._linkifier.reset();
        WebGLAgent.dropTraceLog(this._profile.traceLogId());
    },

    get statusBarItems()
    {
        return [];
    },

    get profile()
    {
        return this._profile;
    },

    wasShown: function()
    {
        var scrollPosition = this._traceLogElementScrollPosition;
        delete this._traceLogElementScrollPosition;
        if (scrollPosition) {
            this._traceLogElement.scrollTop = scrollPosition.top;
            this._traceLogElement.scrollLeft = scrollPosition.left;
        }
    },

    willHide: function()
    {
        this._traceLogElementScrollPosition = {
            top: this._traceLogElement.scrollTop,
            left: this._traceLogElement.scrollLeft
        };
    },

    _showTraceLog: function()
    {
        function didReceiveTraceLog(error, traceLog)
        {
            this._traceLogElement.innerHTML = "";
            if (!traceLog)
                return;
            var calls = traceLog.calls;
            for (var i = 0, n = calls.length; i < n; ++i) {
                var call = calls[i];
                var traceLogItem = document.createElement("div");
                traceLogItem.traceLogId = traceLog.id;
                traceLogItem.stepNo = i;
                traceLogItem.appendChild(document.createTextNode("(" + (i+1) + ") "));

                if (call.sourceURL) {
                    // FIXME(62725): stack trace line/column numbers are one-based.
                    var lineNumber = Math.max(0, call.lineNumber - 1) || 0;
                    var columnNumber = Math.max(0, call.columnNumber - 1) || 0;
                    var linkElement = this._linkifier.linkifyLocation(call.sourceURL, lineNumber, columnNumber);
                    linkElement.textContent = call.functionName;
                    traceLogItem.appendChild(linkElement);
                } else
                    traceLogItem.appendChild(document.createTextNode(call.functionName));

                traceLogItem.appendChild(document.createTextNode("(" + call.arguments.join(", ") + ")"));
                if (typeof call.result !== "undefined")
                    traceLogItem.appendChild(document.createTextNode(" => " + call.result));
                this._traceLogElement.appendChild(traceLogItem);
            }
        }
        WebGLAgent.getTraceLog(this._profile.traceLogId(), didReceiveTraceLog.bind(this));
    },

    _onTraceLogItemClick: function(e)
    {
        var item = e.target;
        if (!item || !item.traceLogId)
            return;
        var time = Date.now();
        function didReplayTraceLog(error, dataURL)
        {
            this._debugInfoElement.textContent = "Replay time: " + (Date.now() - time) + "ms";
            if (this._activeTraceLogItem)
                this._activeTraceLogItem.style.backgroundColor = "";
            this._activeTraceLogItem = item;
            this._activeTraceLogItem.style.backgroundColor = "yellow";
            this._replayImageElement.src = dataURL;
        }
        WebGLAgent.replayTraceLog(item.traceLogId, item.stepNo, didReplayTraceLog.bind(this));
    }
}

WebInspector.WebGLProfileView.prototype.__proto__ = WebInspector.View.prototype;

/**
 * @constructor
 * @extends {WebInspector.ProfileType}
 */
WebInspector.WebGLProfileType = function()
{
    WebInspector.ProfileType.call(this, WebInspector.WebGLProfileType.TypeId, WebInspector.UIString("Capture WebGL Frame"));
    this._nextProfileUid = 1;
    // FIXME: enable/disable by a UI action?
    WebGLAgent.enable();
}

WebInspector.WebGLProfileType.TypeId = "WEBGL_PROFILE";

WebInspector.WebGLProfileType.prototype = {
    get buttonTooltip()
    {
        return WebInspector.UIString("Capture WebGL frame.");
    },

    /**
     * @override
     * @param {WebInspector.ProfilesPanel} profilesPanel
     * @return {boolean}
     */
    buttonClicked: function(profilesPanel)
    {
        var profileHeader = new WebInspector.WebGLProfileHeader(this, WebInspector.UIString("Trace Log %d", this._nextProfileUid), this._nextProfileUid);
        ++this._nextProfileUid;
        profileHeader.isTemporary = true;
        profilesPanel.addProfileHeader(profileHeader);
        function didStartCapturingFrame(error, traceLogId)
        {
            profileHeader._traceLogId = traceLogId;
            profileHeader.isTemporary = false;
        }
        WebGLAgent.captureFrame(didStartCapturingFrame.bind(this));
        return false;
    },

    get treeItemTitle()
    {
        return WebInspector.UIString("WEBGL PROFILE");
    },

    get description()
    {
        return WebInspector.UIString("WebGL calls instrumentation");
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
        return new WebInspector.WebGLProfileHeader(this, title);
    },

    /**
     * @override
     * @param {ProfilerAgent.ProfileHeader} profile
     * @return {WebInspector.ProfileHeader}
     */
    createProfile: function(profile)
    {
        return new WebInspector.WebGLProfileHeader(this, profile.title, -1);
    }
}

WebInspector.WebGLProfileType.prototype.__proto__ = WebInspector.ProfileType.prototype;

/**
 * @constructor
 * @extends {WebInspector.ProfileHeader}
 * @param {WebInspector.WebGLProfileType} type
 * @param {string} title
 * @param {number=} uid
 */
WebInspector.WebGLProfileHeader = function(type, title, uid)
{
    WebInspector.ProfileHeader.call(this, type, title, uid);

    /**
     * @type {string?}
     */
    this._traceLogId = null;
}

WebInspector.WebGLProfileHeader.prototype = {
    /**
     * @return {string?}
     */
    traceLogId: function() {
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
        return new WebInspector.WebGLProfileView(this);
    }
}

WebInspector.WebGLProfileHeader.prototype.__proto__ = WebInspector.ProfileHeader.prototype;
