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
 * @extends {WebInspector.UISourceCode}
 * @param {string} url
 * @param {WebInspector.Resource} resource
 * @param {WebInspector.ContentProvider} contentProvider
 */
WebInspector.JavaScriptSource = function(url, resource, contentProvider, isEditable)
{
    WebInspector.UISourceCode.call(this, url, resource, contentProvider);
    this._isEditable = isEditable;
}

WebInspector.JavaScriptSource.prototype = {
    /**
     * @param {number} lineNumber
     * @param {number} columnNumber
     * @return {WebInspector.DebuggerModel.Location}
     */
    uiLocationToRawLocation: function(lineNumber, columnNumber)
    {
        var rawLocation = WebInspector.UISourceCode.prototype.uiLocationToRawLocation.call(this, lineNumber, columnNumber);
        var debuggerModelLocation = /** @type {WebInspector.DebuggerModel.Location} */ rawLocation;
        return debuggerModelLocation;
    },

    /**
     * @return {boolean}
     */
    supportsEnabledBreakpointsWhileEditing: function()
    {
        return false;
    },

    /**
     * @return {string}
     */
    breakpointStorageId: function()
    {
        return this.formatted() ? "deobfuscated:" + this.url : this.url;
    },

    /**
     * @return {boolean}
     */
    isEditable: function()
    {
        return this._isEditable && WebInspector.debuggerModel.canSetScriptSource();
    },

    /**
     * @return {boolean}
     */
    isDivergedFromVM: function()
    {
        // FIXME: We should return true if this._isDivergedFromVM is set as well once we provide a way to set breakpoints after LiveEdit failure.
        return this.isDirty();
    },

    /**
     * @param {function(?string)} callback
     */
    workingCopyCommitted: function(callback)
    {
        /**
         * @param {?string} error
         */
        function innerCallback(error)
        {
            this._isDivergedFromVM = !!error;
            callback(error);
        }

        var rawLocation = this.uiLocationToRawLocation(0, 0);
        var script = WebInspector.debuggerModel.scriptForId(rawLocation.scriptId);
        WebInspector.debuggerModel.setScriptSource(script.scriptId, this.workingCopy(), innerCallback.bind(this));
    },

    /**
     * @param {string} query
     * @param {boolean} caseSensitive
     * @param {boolean} isRegex
     * @param {function(Array.<WebInspector.ContentProvider.SearchMatch>)} callback
     */
    searchInContent: function(query, caseSensitive, isRegex, callback)
    {
        var content = this.content();
        var provider = content ? new WebInspector.StaticContentProvider(this._contentProvider.contentType(), content) : this._contentProvider;
        provider.searchInContent(query, caseSensitive, isRegex, callback);
    },

    formattedChanged: function()
    {
        WebInspector.breakpointManager.restoreBreakpoints(this);
    }
}

WebInspector.JavaScriptSource.prototype.__proto__ = WebInspector.UISourceCode.prototype;
