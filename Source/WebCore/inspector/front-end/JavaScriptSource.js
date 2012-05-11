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
 * @param {string} id
 * @param {string} url
 * @param {WebInspector.ContentProvider} contentProvider
 * @param {WebInspector.SourceMapping} sourceMapping
 */
WebInspector.JavaScriptSource = function(id, url, contentProvider, sourceMapping)
{
    WebInspector.UISourceCode.call(this, id, url, contentProvider, sourceMapping);

    this._formatterMapping = new WebInspector.IdentityFormatterSourceMapping();
    // FIXME: postpone breakpoints restore to after the mapping has been established.
    setTimeout(function() {
        if (!this._formatted)
            WebInspector.breakpointManager.restoreBreakpoints(this);
    }.bind(this), 0);
}

WebInspector.JavaScriptSource.prototype = {
    /**
     * @param {?string} content
     * @param {boolean} contentEncoded
     * @param {string} mimeType
     */
    fireContentAvailable: function(content, contentEncoded, mimeType)
    {
        WebInspector.UISourceCode.prototype.fireContentAvailable.call(this, content, contentEncoded, mimeType);
        if (this._formatOnLoad) {
            delete this._formatOnLoad;
            this.setFormatted(true);
        }
    },

    /**
     * @param {boolean} formatted
     * @param {function()=} callback
     */
    setFormatted: function(formatted, callback)
    {
        callback = callback || function() {};
        if (!this.contentLoaded()) {
            this._formatOnLoad = formatted;
            callback();
            return;
        }

        if (this._formatted === formatted) {
            callback();
            return;
        }

        this._formatted = formatted;

        // Re-request content
        this._contentLoaded = false;
        WebInspector.UISourceCode.prototype.requestContent.call(this, didGetContent.bind(this));
  
        /**
         * @this {WebInspector.UISourceCode}
         * @param {?string} content
         * @param {boolean} contentEncoded
         * @param {string} mimeType
         */
        function didGetContent(content, contentEncoded, mimeType)
        {
            if (!formatted) {
                this._togglingFormatter = true;
                this.contentChanged(content || "");
                delete this._togglingFormatter;
                this._formatterMapping = new WebInspector.IdentityFormatterSourceMapping();
                this.updateLiveLocations();
                callback();
                return;
            }
    
            var formatter = new WebInspector.ScriptFormatter();
            formatter.formatContent(mimeType, content || "", didFormatContent.bind(this));
  
            /**
             * @this {WebInspector.UISourceCode}
             * @param {string} formattedContent
             * @param {WebInspector.FormatterSourceMapping} formatterMapping
             */
            function didFormatContent(formattedContent, formatterMapping)
            {
                this._togglingFormatter = true;
                this.contentChanged(formattedContent);
                delete this._togglingFormatter;
                this._formatterMapping = formatterMapping;
                this.updateLiveLocations();
                WebInspector.breakpointManager.restoreBreakpoints(this);
                callback();
            }
        }
    },

    /**
     * @return {boolean}
     */
    togglingFormatter: function()
    {
        return this._togglingFormatter;
    },

    /**
     * @param {number} lineNumber
     * @param {number} columnNumber
     * @return {DebuggerAgent.Location}
     */
    uiLocationToRawLocation: function(lineNumber, columnNumber)
    {
        var location = this._formatterMapping.formattedToOriginal(lineNumber, columnNumber);
        return WebInspector.UISourceCode.prototype.uiLocationToRawLocation.call(this, location[0], location[1]);
    },

    /**
     * @param {WebInspector.UILocation} uiLocation
     */
    overrideLocation: function(uiLocation)
    {
        var location = this._formatterMapping.originalToFormatted(uiLocation.lineNumber, uiLocation.columnNumber);
        uiLocation.lineNumber = location[0];
        uiLocation.columnNumber = location[1];
        return uiLocation;
    },

    /**
     * @return {string}
     */
    breakpointStorageId: function()
    {
        return this._formatted ? "deobfuscated:" + this.url : this.url;
    }
}

WebInspector.JavaScriptSource.prototype.__proto__ = WebInspector.UISourceCode.prototype;
