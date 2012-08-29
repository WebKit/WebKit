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
 * @implements {WebInspector.SourceMapping}
 * @param {WebInspector.Workspace} workspace
 */
WebInspector.SASSSourceMapping = function(workspace)
{
    this._workspace = workspace;
    this._uiSourceCodeForURL = {};
    this._uiLocations = {};
    WebInspector.resourceTreeModel.addEventListener(WebInspector.ResourceTreeModel.EventTypes.ResourceAdded, this._resourceAdded, this);
    this._workspace.addEventListener(WebInspector.Workspace.Events.ProjectWillReset, this._reset, this);
}

WebInspector.SASSSourceMapping.prototype = {
    _populate: function()
    {
        function populateFrame(frame)
        {
            for (var i = 0; i < frame.childFrames.length; ++i)
                populateFrame.call(this, frame.childFrames[i]);

            var resources = frame.resources();
            for (var i = 0; i < resources.length; ++i)
                this._resourceAdded({data:resources[i]});
        }

        populateFrame.call(this, WebInspector.resourceTreeModel.mainFrame);
    },

    /**
     * @param {WebInspector.Event} event
     */
    _resourceAdded: function(event)
    {
        var resource = /** @type {WebInspector.Resource} */ event.data;
        if (resource.type !== WebInspector.resourceTypes.Stylesheet)
            return;

        /**
         * @param {?string} content
         * @param {boolean} contentEncoded
         * @param {string} mimeType
         */
        function didRequestContent(content, contentEncoded, mimeType)
        {
            if (!content)
                return;
            var lines = content.split(/\r?\n/);
            var debugInfoRegex = /@media\s\-sass\-debug\-info{filename{font-family:([^}]+)}line{font-family:\\[0]+([^}]*)}}/i;
            var lineNumbersRegex = /\/\*\s+line\s+([0-9]+),\s+([^*\/]+)/;
            for (var lineNumber = 0; lineNumber < lines.length; ++lineNumber) {
                var match = debugInfoRegex.exec(lines[lineNumber]);
                if (match) {
                    var url = match[1].replace(/\\(.)/g, "$1");
                    var line = parseInt(decodeURI(match[2].replace(/(..)/g, "%$1")), 10);
                    this._bindUISourceCode(url, line, resource.url, lineNumber);
                    continue;
                }
                match = lineNumbersRegex.exec(lines[lineNumber]);
                if (match) {
                    var fileName = match[2].trim();
                    var line = parseInt(match[1], 10);
                    var url = resource.url;
                    if (url.endsWith("/" + resource.parsedURL.lastPathComponent))
                        url = url.substring(0, url.length - resource.parsedURL.lastPathComponent.length) + fileName;
                    else
                        url = fileName;
                    this._bindUISourceCode(url, line, resource.url, lineNumber);
                    continue;
                }
            }
        }
        resource.requestContent(didRequestContent.bind(this));
    },

    /**
     * @param {string} url
     * @param {number} line
     * @param {string} rawURL
     * @param {number} rawLine
     */
    _bindUISourceCode: function(url, line, rawURL, rawLine)
    {
        var uiSourceCode = this._uiSourceCodeForURL[url];
        if (!uiSourceCode) {
            uiSourceCode = new WebInspector.SASSSource(url);
            this._uiSourceCodeForURL[url] = uiSourceCode;
            this._workspace.project().addUISourceCode(uiSourceCode);
            WebInspector.cssModel.setSourceMapping(rawURL, this);
        }
        var rawLocationString = rawURL + ":" + (rawLine + 1);  // Next line after mapping metainfo
        this._uiLocations[rawLocationString] = new WebInspector.UILocation(uiSourceCode, line - 1, 0);
    },

    /**
     * @param {WebInspector.RawLocation} rawLocation
     * @return {WebInspector.UILocation}
     */
    rawLocationToUILocation: function(rawLocation)
    {
        var location = /** @type WebInspector.CSSLocation */ rawLocation;
        var uiLocation = this._uiLocations[location.url + ":" + location.lineNumber];
        if (!uiLocation) {
            var uiSourceCode = this._workspace.uiSourceCodeForURL(location.url);
            uiLocation = new WebInspector.UILocation(uiSourceCode, location.lineNumber, 0);
        }
        return uiLocation;
    },

    /**
     * @param {WebInspector.UISourceCode} uiSourceCode
     * @param {number} lineNumber
     * @param {number} columnNumber
     * @return {WebInspector.RawLocation}
     */
    uiLocationToRawLocation: function(uiSourceCode, lineNumber, columnNumber)
    {
        // FIXME: Implement this when ui -> raw mapping has clients.
        return new WebInspector.CSSLocation(uiSourceCode.contentURL() || "", lineNumber);
    },

    _reset: function()
    {
        this._uiSourceCodeForURL = {};
        this._uiLocations = {};
        this._populate();
    }
}

/**
 * @constructor
 * @extends {WebInspector.UISourceCode}
 * @param {string} sassURL
 */
WebInspector.SASSSource = function(sassURL)
{
    var content = InspectorFrontendHost.loadResourceSynchronously(sassURL);
    var contentProvider = new WebInspector.StaticContentProvider(WebInspector.resourceTypes.Stylesheet, content);
    WebInspector.UISourceCode.call(this, sassURL, null, contentProvider);
}

WebInspector.SASSSource.prototype = {
    /**
     * @return {boolean}
     */
    isEditable: function()
    {
        return true;
    }
}

WebInspector.SASSSource.prototype.__proto__ = WebInspector.UISourceCode.prototype;
