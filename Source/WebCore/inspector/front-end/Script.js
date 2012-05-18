/*
 * Copyright (C) 2008 Apple Inc. All Rights Reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/**
 * @constructor
 * @implements {WebInspector.ContentProvider}
 * @param {string} scriptId
 * @param {string} sourceURL
 * @param {number} startLine
 * @param {number} startColumn
 * @param {number} endLine
 * @param {number} endColumn
 * @param {boolean} isContentScript
 * @param {string=} sourceMapURL
 */
WebInspector.Script = function(scriptId, sourceURL, startLine, startColumn, endLine, endColumn, isContentScript, sourceMapURL)
{
    this.scriptId = scriptId;
    this.sourceURL = sourceURL;
    this.lineOffset = startLine;
    this.columnOffset = startColumn;
    this.endLine = endLine;
    this.endColumn = endColumn;
    this.isContentScript = isContentScript;
    this.sourceMapURL = sourceMapURL;
    this._locations = [];
}

WebInspector.Script.prototype = {
    /**
     * @return {?string}
     */
    contentURL: function()
    {
        return this.sourceURL;
    },

    /**
     * @return {WebInspector.ResourceType}
     */
    contentType: function()
    {
        return WebInspector.resourceTypes.Script;
    },

    /**
     * @param {function(?string,boolean,string)} callback
     */
    requestContent: function(callback)
    {
        if (this._source) {
            callback(this._source, false, "text/javascript");
            return;
        }

        /**
         * @this {WebInspector.Script}
         * @param {?Protocol.Error} error
         * @param {string} source
         */
        function didGetScriptSource(error, source)
        {
            this._source = error ? "" : source;
            callback(this._source, false, "text/javascript");
        }
        if (this.scriptId) {
            // Script failed to parse.
            DebuggerAgent.getScriptSource(this.scriptId, didGetScriptSource.bind(this));
        } else
            callback("", false, "text/javascript");
    },

    /**
     * @param {string} query
     * @param {boolean} caseSensitive
     * @param {boolean} isRegex
     * @param {function(Array.<PageAgent.SearchMatch>)} callback
     */
    searchInContent: function(query, caseSensitive, isRegex, callback)
    {
        /**
         * @this {WebInspector.Script}
         * @param {?Protocol.Error} error
         * @param {Array.<PageAgent.SearchMatch>} searchMatches
         */
        function innerCallback(error, searchMatches)
        {
            if (error)
                console.error(error);
            var result = [];
            for (var i = 0; i < searchMatches.length; ++i) {
                var searchMatch = new WebInspector.ContentProvider.SearchMatch(searchMatches[i].lineNumber, searchMatches[i].lineContent);
                result.push(searchMatch);
            }
            callback(result || []);
        }

        if (this.scriptId) {
            // Script failed to parse.
            DebuggerAgent.searchInContent(this.scriptId, query, caseSensitive, isRegex, innerCallback.bind(this));
        } else
            callback([]);
    },

    /**
     * @param {string} newSource
     * @param {function(?Protocol.Error, Array.<DebuggerAgent.CallFrame>=)} callback
     */
    editSource: function(newSource, callback)
    {
        /**
         * @this {WebInspector.Script}
         * @param {?Protocol.Error} error
         * @param {Array.<DebuggerAgent.CallFrame>|undefined} callFrames
         * @param {Object=} debugData
         */
        function didEditScriptSource(error, callFrames, debugData)
        {
            if (!error)
                this._source = newSource;
            callback(error, callFrames);
        }
        if (this.scriptId) {
            // Script failed to parse.
            DebuggerAgent.setScriptSource(this.scriptId, newSource, undefined, didEditScriptSource.bind(this));
        } else
            callback("Script failed to parse");
    },

    /**
     * @return {boolean}
     */
    isInlineScript: function()
    {
        var startsAtZero = !this.lineOffset && !this.columnOffset;
        return !!this.sourceURL && !startsAtZero;
    },

    /**
     * @param {DebuggerAgent.Location} rawLocation
     * @return {WebInspector.UILocation}
     */
    rawLocationToUILocation: function(rawLocation)
    {
        console.assert(rawLocation.scriptId === this.scriptId);
        var uiLocation = this._sourceMapping.rawLocationToUILocation(rawLocation);
        return uiLocation.uiSourceCode.overrideLocation(uiLocation);
    },

    /**
     * @param {WebInspector.SourceMapping} sourceMapping
     */
    setSourceMapping: function(sourceMapping)
    {
        this._sourceMapping = sourceMapping;
        for (var i = 0; i < this._locations.length; ++i)
            this._locations[i].update();
    },

    /**
     * @param {DebuggerAgent.Location} rawLocation
     * @param {function(WebInspector.UILocation):(boolean|undefined)} updateDelegate
     * @return {WebInspector.Script.Location}
     */
    createLiveLocation: function(rawLocation, updateDelegate)
    {
        console.assert(rawLocation.scriptId === this.scriptId);
        var location = new WebInspector.Script.Location(this, rawLocation, updateDelegate);
        this._locations.push(location);
        location.update();
        return location;
    }
}

/**
 * @constructor
 * @param {WebInspector.Script} script
 * @param {DebuggerAgent.Location} rawLocation
 * @param {function(WebInspector.UILocation):(boolean|undefined)} updateDelegate
 */
WebInspector.Script.Location = function(script, rawLocation, updateDelegate)
{
    this._script = script;
    this._rawLocation = rawLocation;
    this._updateDelegate = updateDelegate;
    this._uiSourceCodes = [];
}

WebInspector.Script.Location.prototype = {
    update: function()
    {
        var uiLocation = this._script.rawLocationToUILocation(this._rawLocation);
        if (uiLocation) {
            var uiSourceCode = uiLocation.uiSourceCode;
            if (this._uiSourceCodes.indexOf(uiSourceCode) === -1) {
                uiSourceCode.addLiveLocation(this);
                this._uiSourceCodes.push(uiSourceCode);
            }
            var oneTime = this._updateDelegate(uiLocation);
            if (oneTime)
                this.dispose();
        }
    },

    dispose: function()
    {
        for (var i = 0; i < this._uiSourceCodes.length; ++i)
            this._uiSourceCodes[i].removeLiveLocation(this);
        this._uiSourceCodes = [];
        this._script._locations.remove(this);
    }
}
