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

// RawSourceCode represents JavaScript resource or HTML resource with inlined scripts
// as it came from network.

/**
 * @constructor
 * @extends {WebInspector.Object}
 */
WebInspector.RawSourceCode = function(id, script, resource, formatter, formatted)
{
    this.id = id;
    this.url = script.sourceURL;
    this.isContentScript = script.isContentScript;
    this._scripts = [script];
    this._formatter = formatter;
    this._formatted = formatted;
    this._resource = resource;
    this.messages = [];

    this._useTemporaryContent = this._resource && !this._resource.finished;
    this._hasNewScripts = true;
    if (!this._useTemporaryContent)
        this._updateSourceMapping();
    else if (this._resource)
        this._resource.addEventListener("finished", this._resourceFinished.bind(this));
}

WebInspector.RawSourceCode.Events = {
    SourceMappingUpdated: "source-mapping-updated"
}

WebInspector.RawSourceCode.prototype = {
    addScript: function(script)
    {
        this._scripts.push(script);
        this._hasNewScripts = true;
    },

    get sourceMapping()
    {
        return this._sourceMapping;
    },

    setFormatted: function(formatted)
    {
        if (this._formatted === formatted)
            return;
        this._formatted = formatted;
        this._updateSourceMapping();
    },

    contentEdited: function()
    {
        this._updateSourceMapping();
    },

    _resourceFinished: function()
    {
        this._useTemporaryContent = false;
        this._updateSourceMapping();
    },

    _scriptForRawLocation: function(lineNumber, columnNumber)
    {
        var closestScript = this._scripts[0];
        for (var i = 1; i < this._scripts.length; ++i) {
            var script = this._scripts[i];
            if (script.lineOffset > lineNumber || (script.lineOffset === lineNumber && script.columnOffset > columnNumber))
                continue;
            if (script.lineOffset > closestScript.lineOffset ||
                (script.lineOffset === closestScript.lineOffset && script.columnOffset > closestScript.columnOffset))
                closestScript = script;
        }
        return closestScript;
    },

    forceUpdateSourceMapping: function(script)
    {
        if (!this._useTemporaryContent || !this._hasNewScripts)
            return;
        this._hasNewScripts = false;
        this._updateSourceMapping();
    },

    _updateSourceMapping: function()
    {
        if (this._updatingSourceMapping) {
            this._updateNeeded = true;
            return;
        }
        this._updatingSourceMapping = true;
        this._updateNeeded = false;

        var originalContentProvider = this._createContentProvider();
        this._createSourceMapping(originalContentProvider, didCreateSourceMapping.bind(this));

        function didCreateSourceMapping(contentProvider, mapping)
        {
            this._updatingSourceMapping = false;
            if (!this._updateNeeded)
                this._saveSourceMapping(contentProvider, mapping);
            else
                this._updateSourceMapping();
        }
    },

    _createContentProvider: function()
    {
        if (this._resource && this._resource.finished)
            return new WebInspector.ResourceContentProvider(this._resource);
        if (this._scripts.length === 1 && !this._scripts[0].lineOffset && !this._scripts[0].columnOffset)
            return new WebInspector.ScriptContentProvider(this._scripts[0]);
        return new WebInspector.ConcatenatedScriptsContentProvider(this._scripts);
    },

    _createSourceMapping: function(originalContentProvider, callback)
    {
        if (!this._formatted) {
            var uiSourceCode = new WebInspector.UISourceCode(this.id, this.url, this.isContentScript, this, originalContentProvider);
            var sourceMapping = new WebInspector.RawSourceCode.PlainSourceMapping(this, uiSourceCode);
            callback(sourceMapping);
            return;
        }

        function didRequestContent(mimeType, content)
        {
            function didFormatContent(formattedContent, mapping)
            {
                var contentProvider = new WebInspector.StaticContentProvider(mimeType, formattedContent)
                var uiSourceCode = new WebInspector.UISourceCode("deobfuscated:" + this.id, this.url, this.isContentScript, this, contentProvider);
                var sourceMapping = new WebInspector.RawSourceCode.FormattedSourceMapping(this, uiSourceCode, mapping);
                callback(sourceMapping);
            }
            this._formatter.formatContent(mimeType, content, didFormatContent.bind(this));
        }
        originalContentProvider.requestContent(didRequestContent.bind(this));
    },

    _saveSourceMapping: function(sourceMapping)
    {
        var oldUISourceCode;
        if (this._sourceMapping)
            oldUISourceCode = this._sourceMapping.uiSourceCode;
        this._sourceMapping = sourceMapping;
        this.dispatchEventToListeners(WebInspector.RawSourceCode.Events.SourceMappingUpdated, { oldUISourceCode: oldUISourceCode });
    }
}

WebInspector.RawSourceCode.prototype.__proto__ = WebInspector.Object.prototype;


/**
 * @constructor
 */
WebInspector.RawSourceCode.PlainSourceMapping = function(rawSourceCode, uiSourceCode)
{
    this._rawSourceCode = rawSourceCode;
    this._uiSourceCode = uiSourceCode;
}

WebInspector.RawSourceCode.PlainSourceMapping.prototype = {
    rawLocationToUILocation: function(rawLocation)
    {
        return new WebInspector.UILocation(this._uiSourceCode, rawLocation.lineNumber, rawLocation.columnNumber);
    },

    uiLocationToRawLocation: function(lineNumber, columnNumber)
    {
        var rawLocation = { lineNumber: lineNumber, columnNumber: columnNumber };
        rawLocation.scriptId = this._rawSourceCode._scriptForRawLocation(rawLocation.lineNumber, rawLocation.columnNumber).scriptId;
        return rawLocation;
    },

    get uiSourceCode()
    {
        return this._uiSourceCode;
    }
}

/**
 * @constructor
 */
WebInspector.RawSourceCode.FormattedSourceMapping = function(rawSourceCode, uiSourceCode, mapping)
{
    this._rawSourceCode = rawSourceCode;
    this._uiSourceCode = uiSourceCode;
    this._mapping = mapping;
}

WebInspector.RawSourceCode.FormattedSourceMapping.prototype = {
    rawLocationToUILocation: function(rawLocation)
    {
        var location = this._mapping.originalToFormatted(rawLocation);
        return new WebInspector.UILocation(this._uiSourceCode, location.lineNumber, location.columnNumber);
    },

    uiLocationToRawLocation: function(lineNumber, columnNumber)
    {
        var rawLocation = this._mapping.formattedToOriginal({ lineNumber: lineNumber, columnNumber: columnNumber });
        rawLocation.scriptId = this._rawSourceCode._scriptForRawLocation(rawLocation.lineNumber, rawLocation.columnNumber).scriptId;
        return rawLocation;
    },

    get uiSourceCode()
    {
        return this._uiSourceCode;
    }
}

/**
 * @constructor
 */
WebInspector.UILocation = function(uiSourceCode, lineNumber, columnNumber)
{
    this.uiSourceCode = uiSourceCode;
    this.lineNumber = lineNumber;
    this.columnNumber = columnNumber;
}
