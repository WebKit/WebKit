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

WebInspector.SourceFile = function(id, script, contentChangedDelegate)
{
    this._scripts = [script];
    this._contentChangedDelegate = contentChangedDelegate;
    if (script.sourceURL)
        this._resource = WebInspector.networkManager.inflightResourceForURL(script.sourceURL) || WebInspector.resourceForURL(script.sourceURL);
    this._requestContentCallbacks = [];

    this.id = id;
    this.url = script.sourceURL;
    this.isExtensionScript = script.worldType === WebInspector.Script.WorldType.EXTENSIONS_WORLD;
    this.breakpoints = {};

    if (this._hasPendingResource())
        this._resource.addEventListener("finished", this.reload.bind(this));
}

WebInspector.SourceFile.prototype = {
    addScript: function(script)
    {
        this._scripts.push(script);
    },

    requestContent: function(callback)
    {
        if (this._contentLoaded) {
            callback(this._mimeType, this._content);
            return;
        }

        this._requestContentCallbacks.push(callback);
        this._requestContent();
    },

    forceLoadContent: function(script)
    {
        if (!this._hasPendingResource())
            return;

        if (!this._concatenatedScripts)
            this._concatenatedScripts = {};
        if (this._concatenatedScripts[script.sourceID])
            return;
        for (var i = 0; i < this._scripts.length; ++i)
            this._concatenatedScripts[this._scripts[i].sourceID] = true;

        this.reload();

        if (!this._contentRequested) {
            this._contentRequested = true;
            this._loadAndConcatenateScriptsContent();
        }
    },

    reload: function()
    {
        if (this._contentLoaded) {
            this._contentLoaded = false;
            this._contentChangedDelegate();
        } else if (this._contentRequested)
            this._reloadContent = true;
        else if (this._requestContentCallbacks.length)
            this._requestContent();
    },

    scriptLocationToSourceLocation: function(lineNumber, columnNumber)
    {
        return { lineNumber: lineNumber, columnNumber: columnNumber };
    },

    sourceLocationToScriptLocation: function(lineNumber, columnNumber)
    {
        var closestScript = this._scripts[0];
        for (var i = 1; i < this._scripts.length; ++i) {
            script = this._scripts[i];
            if (script.lineOffset > lineNumber || (script.lineOffset === lineNumber && script.columnOffset > columnNumber))
                break;
            closestScript = script;
        }
        return { scriptId: closestScript.sourceID, lineNumber: lineNumber, columnNumber: columnNumber };
    },

    _requestContent: function()
    {
        if (this._contentRequested)
            return;

        this._contentRequested = true;
        if (this._resource && this._resource.finished)
            this._loadResourceContent();
        else if (!this._resource)
            this._loadScriptContent();
        else if (this._concatenatedScripts)
            this._loadAndConcatenateScriptsContent();
        else
            this._contentRequested = false;
    },

    _loadResourceContent: function()
    {
        // FIXME: move provider's load functions here.
        var provider = new WebInspector.SourceFrameDelegateForScriptsPanel(null, null, this._scripts[0]);
        provider._loadResourceContent(this._resource, this._didRequestContent.bind(this));
    },

    _loadScriptContent: function()
    {
        this._scripts[0].requestSource(this._didRequestContent.bind(this, "text/javascript"));
    },

    _loadAndConcatenateScriptsContent: function()
    {
        // FIXME: move provider's load functions here.
        var provider = new WebInspector.SourceFrameDelegateForScriptsPanel(null, null, this._scripts[0]);
        provider._loadAndConcatenateScriptsContent(this._didRequestContent.bind(this));
    },

    _didRequestContent: function(mimeType, content)
    {
        this._contentLoaded = true;
        this._contentRequested = false;
        this._mimeType = mimeType;
        this._content = content;

        for (var i = 0; i < this._requestContentCallbacks.length; ++i)
            this._requestContentCallbacks[i](mimeType, content);
        this._requestContentCallbacks = [];

        if (this._reloadContent)
            this.reload();
    },

    _hasPendingResource: function()
    {
        return this._resource && !this._resource.finished;
    }
}

WebInspector.FormattedSourceFile = function(sourceFileId, script, contentChangedDelegate, formatter)
{
    WebInspector.SourceFile.call(this, sourceFileId, script, contentChangedDelegate);
    this._formatter = formatter;
}

WebInspector.FormattedSourceFile.prototype = {
    scriptLocationToSourceLocation: function(lineNumber, columnNumber)
    {
        // FIXME: this method should force content formatting to obtain the mapping.
        if (!this._originalToFormatted)
            return WebInspector.SourceFile.prototype.scriptLocationToSourceLocation.call(this, lineNumber, columnNumber);
        return this._originalToFormatted(lineNumber, columnNumber);
    },

    sourceLocationToScriptLocation: function(lineNumber, columnNumber)
    {
        // FIXME: this method should force content formatting to obtain the mapping.
        if (!this._formattedToOriginal)
            return WebInspector.SourceFile.prototype.sourceLocationToScriptLocation.call(this, lineNumber, columnNumber);
        var location = this._formattedToOriginal(lineNumber, columnNumber);
        return WebInspector.SourceFile.prototype.sourceLocationToScriptLocation.call(this, location.lineNumber, location.columnNumber);
    },

    _didRequestContent: function(mimeType, text)
    {
        function didFormatContent(text, originalToFormatted, formattedToOriginal)
        {
            this._originalToFormatted = originalToFormatted;
            this._formattedToOriginal = formattedToOriginal;
            WebInspector.SourceFile.prototype._didRequestContent.call(this, mimeType, text);
        }
        this._formatter.formatContent(text, this._scripts, didFormatContent.bind(this));
    }
}

WebInspector.FormattedSourceFile.prototype.__proto__ = WebInspector.SourceFile.prototype;
