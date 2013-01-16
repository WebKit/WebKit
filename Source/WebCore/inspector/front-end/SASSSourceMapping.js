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
 * @param {WebInspector.SimpleWorkspaceProvider} networkWorkspaceProvider
 */
WebInspector.SASSSourceMapping = function(workspace, networkWorkspaceProvider)
{
    this._workspace = workspace;
    this._networkWorkspaceProvider = networkWorkspaceProvider;
    this._uiLocations = {};
    this._cssURLsForSASSURL = {};
    this._timeoutForURL = {};
    WebInspector.resourceTreeModel.addEventListener(WebInspector.ResourceTreeModel.EventTypes.ResourceAdded, this._resourceAdded, this);
    WebInspector.fileManager.addEventListener(WebInspector.FileManager.EventTypes.SavedURL, this._fileSaveFinished, this);
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
    _fileSaveFinished: function(event)
    {
        var sassURL = /** @type {string} */ (event.data);
        function callback()
        {
            delete this._timeoutForURL[sassURL];
            var cssURLs = this._cssURLsForSASSURL[sassURL];
            if (!cssURLs)
                return;
            for (var i = 0; i < cssURLs.length; ++i)
                this._reloadCSS(cssURLs[i]);
        }

        var timer = this._timeoutForURL[sassURL];
        if (timer) {
            clearTimeout(timer);
            delete this._timeoutForURL[sassURL];
        }
        if (!WebInspector.settings.cssReloadEnabled.get() || !this._cssURLsForSASSURL[sassURL])
            return;
        var timeout = WebInspector.settings.cssReloadTimeout.get();
        if (timeout && isFinite(timeout))
            this._timeoutForURL[sassURL] = setTimeout(callback.bind(this), Number(timeout));
    },

    _reloadCSS: function(url)
    {
        var uiSourceCode = this._workspace.uiSourceCodeForURL(url);
        if (!uiSourceCode)
            return;
        var newContent = InspectorFrontendHost.loadResourceSynchronously(url);
        uiSourceCode.addRevision(newContent);
    },

    /**
     * @param {WebInspector.Event} event
     */
    _resourceAdded: function(event)
    {
        var resource = /** @type {WebInspector.Resource} */ (event.data);
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
            var debugInfoRegex = /@media\s\-sass\-debug\-info{filename{font-family:([^}]+)}line{font-family:\\0000(\d\d)([^}]*)}}/i;
            var lineNumbersRegex = /\/\*\s+line\s+([0-9]+),\s+([^*\/]+)/;
            for (var lineNumber = 0; lineNumber < lines.length; ++lineNumber) {
                var match = debugInfoRegex.exec(lines[lineNumber]);
                if (match) {
                    var url = match[1].replace(/\\(.)/g, "$1");
                    var line = parseInt(decodeURI("%" + match[2]) + match[3], 10);
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
        var uiSourceCode = this._workspace.uiSourceCodeForURL(url);
        if (!uiSourceCode) {
            var content = InspectorFrontendHost.loadResourceSynchronously(url);
            var contentProvider = new WebInspector.StaticContentProvider(WebInspector.resourceTypes.Stylesheet, content, "text/x-scss");
            uiSourceCode = this._networkWorkspaceProvider.addFileForURL(url, contentProvider, true);
            WebInspector.cssModel.setSourceMapping(rawURL, this);
        }
        var rawLocationString = rawURL + ":" + (rawLine + 1);  // Next line after mapping metainfo
        this._uiLocations[rawLocationString] = new WebInspector.UILocation(uiSourceCode, line - 1, 0);
        this._addCSSURLforSASSURL(rawURL, url);
    },

    /**
     * @param {string} cssURL
     * @param {string} sassURL
     */
    _addCSSURLforSASSURL: function(cssURL, sassURL)
    {
        var cssURLs;
        if (this._cssURLsForSASSURL.hasOwnProperty(sassURL))
            cssURLs = this._cssURLsForSASSURL[sassURL];
        else {
            cssURLs = [];
            this._cssURLsForSASSURL[sassURL] = cssURLs;
        }
        if (cssURLs.indexOf(cssURL) === -1)
            cssURLs.push(cssURL);
    },

    /**
     * @param {WebInspector.RawLocation} rawLocation
     * @return {WebInspector.UILocation}
     */
    rawLocationToUILocation: function(rawLocation)
    {
        var location = /** @type WebInspector.CSSLocation */ (rawLocation);
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
        return new WebInspector.CSSLocation(uiSourceCode.url || "", lineNumber);
    },

    _reset: function()
    {
        this._uiLocations = {};
        this._populate();
    }
}

