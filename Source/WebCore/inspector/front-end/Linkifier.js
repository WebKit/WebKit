/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
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
 */
WebInspector.Linkifier = function(debuggerPresentationModel)
{
    this._debuggerPresentationModel = debuggerPresentationModel;
    this._anchorsForURL = {};
}

WebInspector.Linkifier.prototype = {
    linkifyLocation: function(sourceURL, lineNumber, columnNumber, classes)
    {
        var linkText = WebInspector.formatLinkText(sourceURL, lineNumber);
        var anchor = WebInspector.linkifyURLAsNode(sourceURL, linkText, classes, false);
        anchor.rawLocation = { lineNumber: lineNumber, columnNumber: columnNumber };

        var rawSourceCode = this._debuggerPresentationModel._rawSourceCodeForScript(sourceURL);
        if (!rawSourceCode) {
            anchor.setAttribute("preferred_panel", "resources");
            anchor.setAttribute("line_number", lineNumber);
            return anchor;
        }

        var anchors = this._anchorsForURL[sourceURL];
        if (!anchors) {
            anchors = [];
            this._anchorsForURL[sourceURL] = anchors;
            rawSourceCode.addEventListener(WebInspector.RawSourceCode.Events.SourceMappingUpdated, this._updateSourceAnchors, this);
        }

        if (rawSourceCode.sourceMapping)
            this._updateAnchor(rawSourceCode, anchor);
        anchors.push(anchor);
        return anchor;
    },

    reset: function()
    {
        for (var sourceURL in this._anchorsForURL) {
            var rawSourceCode = this._debuggerPresentationModel._rawSourceCodeForScript(sourceURL);
            if (rawSourceCode)
                rawSourceCode.removeEventListener(WebInspector.RawSourceCode.Events.SourceMappingUpdated, this._updateSourceAncors, this);
        }
        this._anchorsForURL = {};
    },

    _updateSourceAnchors: function(event)
    {
        var rawSourceCode = event.target;
        var anchors = this._anchorsForURL[rawSourceCode.url];
        if (!anchors)
            return;

        for (var i = 0; i < anchors.length; ++i)
            this._updateAnchor(rawSourceCode, anchors[i]);
    },

    _updateAnchor: function(rawSourceCode, anchor)
    {
        var uiLocation = rawSourceCode.sourceMapping.rawLocationToUILocation(anchor.rawLocation);
        anchor.textContent = WebInspector.formatLinkText(uiLocation.uiSourceCode.url, uiLocation.lineNumber);
        anchor.setAttribute("preferred_panel", "scripts");
        anchor.uiSourceCode = uiLocation.uiSourceCode;
        anchor.lineNumber = uiLocation.lineNumber;
    }
}

