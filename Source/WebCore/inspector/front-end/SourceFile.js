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
WebInspector.RawSourceCode = function(id, script, formatter)
{
    this._scripts = [script];
    this._formatter = formatter;
    this._formatted = false;

    if (script.sourceURL)
        this._resource = WebInspector.networkManager.inflightResourceForURL(script.sourceURL) || WebInspector.resourceForURL(script.sourceURL);
    this._requestContentCallbacks = [];

    this.id = id;
    this.url = script.sourceURL;
    this.isContentScript = script.isContentScript;
    this.messages = [];

    if (this._hasPendingResource())
        this._resource.addEventListener("finished", this.reload.bind(this));
}

WebInspector.RawSourceCode.Events = {
    UISourceCodeReplaced: "ui-source-code-replaced"
}

WebInspector.RawSourceCode.prototype = {
    addScript: function(script)
    {
        this._scripts.push(script);
    },

    get uiSourceCode()
    {
        // FIXME: extract UISourceCode from RawSourceCode (currently RawSourceCode implements methods from both interfaces).
        return this;
    },

    get rawSourceCode()
    {
        // FIXME: extract UISourceCode from RawSourceCode (currently RawSourceCode implements methods from both interfaces).
        return this;
    },

    rawLocationToUILocation: function(rawLocation)
    {
        var uiLocation = this._mapping ? this._mapping.originalToFormatted(rawLocation) : rawLocation;
        uiLocation.sourceFile = this;
        return uiLocation;
    },

    uiLocationToRawLocation: function(lineNumber, columnNumber)
    {
        var rawLocation = { lineNumber: lineNumber, columnNumber: columnNumber };
        if (this._mapping)
            rawLocation = this._mapping.formattedToOriginal(rawLocation);
        rawLocation.scriptId = this._scriptForRawLocation(rawLocation.lineNumber, rawLocation.columnNumber).scriptId;
        return rawLocation;
    },

    _scriptForRawLocation: function(lineNumber, columnNumber)
    {
        var closestScript = this._scripts[0];
        for (var i = 1; i < this._scripts.length; ++i) {
            script = this._scripts[i];
            if (script.lineOffset > lineNumber || (script.lineOffset === lineNumber && script.columnOffset > columnNumber))
                continue;
            if (script.lineOffset > closestScript.lineOffset ||
                (script.lineOffset === closestScript.lineOffset && script.columnOffset > closestScript.columnOffset))
                closestScript = script;
        }
        return closestScript;
    },

    setFormatted: function(formatted)
    {
        // FIXME: this should initiate formatting and trigger events to update ui.
        this._formatted = formatted;
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

    get content()
    {
        return this._content;
    },

    set content(content)
    {
        // FIXME: move live edit implementation to SourceFile and remove this setter.
        this._content = content;
    },

    createSourceMappingIfNeeded: function(callback)
    {
        if (!this._formatted) {
            callback();
            return;
        }

        function didRequestContent()
        {
            callback();
        }
        // Force content formatting to obtain the mapping.
        this.requestContent(didRequestContent.bind(this));
    },

    forceLoadContent: function(script)
    {
        if (!this._hasPendingResource())
            return;

        if (!this._concatenatedScripts)
            this._concatenatedScripts = {};
        if (this._concatenatedScripts[script.scriptId])
            return;
        for (var i = 0; i < this._scripts.length; ++i)
            this._concatenatedScripts[this._scripts[i].scriptId] = true;

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
            // FIXME: create another UISourceCode instance here, UISourceCode should be immutable.
            this.dispatchEventToListeners(WebInspector.RawSourceCode.Events.UISourceCodeReplaced, { oldSourceCode: this, sourceCode: this });
        } else if (this._contentRequested)
            this._reloadContent = true;
        else if (this._requestContentCallbacks.length)
            this._requestContent();
    },

    _requestContent: function()
    {
        if (this._contentRequested)
            return;

        this._contentRequested = true;
        if (this._resource && this._resource.finished)
            this._loadResourceContent(this._resource);
        else if (!this._resource)
            this._loadScriptContent();
        else if (this._concatenatedScripts)
            this._loadAndConcatenateScriptsContent();
        else
            this._contentRequested = false;
    },

    _loadResourceContent: function(resource)
    {
        var contentProvider = new WebInspector.ResourceContentProvider(resource);
        contentProvider.requestContent(this._didRequestContent.bind(this));
    },

    _loadScriptContent: function()
    {
        var contentProvider = new WebInspector.ScriptContentProvider(this._scripts[0]);
        contentProvider.requestContent(this._didRequestContent.bind(this));
    },

    _loadAndConcatenateScriptsContent: function()
    {
        var contentProvider;
        if (this._scripts.length === 1 && !this._scripts[0].lineOffset && !this._scripts[0].columnOffset)
            contentProvider = new WebInspector.ScriptContentProvider(this._scripts[0]);
        else
            contentProvider = new WebInspector.ConcatenatedScriptsContentProvider(this._scripts);
        contentProvider.requestContent(this._didRequestContent.bind(this));
    },

    _didRequestContent: function(mimeType, content)
    {
        if (!this._formatted) {
            this._invokeRequestContentCallbacks(mimeType, content);
            return;
        }

        function didFormatContent(formattedContent, mapping)
        {
            this._mapping = mapping;
            this._invokeRequestContentCallbacks(mimeType, formattedContent);
        }
        this._formatter.formatContent(mimeType, content, didFormatContent.bind(this));
    },

    _invokeRequestContentCallbacks: function(mimeType, content)
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

WebInspector.RawSourceCode.prototype.__proto__ = WebInspector.Object.prototype;


WebInspector.ScriptContentProvider = function(script)
{
    this._mimeType = "text/javascript";
    this._script = script;
};

WebInspector.ScriptContentProvider.prototype = {
requestContent: function(callback)
{
    function didRequestSource(source)
    {
        callback(this._mimeType, source);
    }
    this._script.requestSource(didRequestSource.bind(this));
}
}

WebInspector.ScriptContentProvider.prototype.__proto__ = WebInspector.ContentProvider.prototype;


WebInspector.ConcatenatedScriptsContentProvider = function(scripts)
{
    this._mimeType = "text/html";
    this._scripts = scripts;
};

WebInspector.ConcatenatedScriptsContentProvider.prototype = {
   requestContent: function(callback)
   {
       var scripts = this._scripts.slice();
       scripts.sort(function(x, y) { return x.lineOffset - y.lineOffset || x.columnOffset - y.columnOffset; });
       var sources = [];
       function didRequestSource(source)
       {
           sources.push(source);
           if (sources.length == scripts.length)
               callback(this._mimeType, this._concatenateScriptsContent(scripts, sources));
       }
       for (var i = 0; i < scripts.length; ++i)
           scripts[i].requestSource(didRequestSource.bind(this));
   },

   _concatenateScriptsContent: function(scripts, sources)
   {
       var content = "";
       var lineNumber = 0;
       var columnNumber = 0;
       var scriptRanges = [];
       function appendChunk(chunk, script)
       {
           var start = { lineNumber: lineNumber, columnNumber: columnNumber };
           content += chunk;
           var lineEndings = chunk.lineEndings();
           var lineCount = lineEndings.length;
           if (lineCount === 1)
               columnNumber += chunk.length;
           else {
               lineNumber += lineCount - 1;
               columnNumber = lineEndings[lineCount - 1] - lineEndings[lineCount - 2] - 1;
           }
           var end = { lineNumber: lineNumber, columnNumber: columnNumber };
           if (script)
               scriptRanges.push({ start: start, end: end, sourceId: script.sourceId });
       }

       var scriptOpenTag = "<script>";
       var scriptCloseTag = "</script>";
       for (var i = 0; i < scripts.length; ++i) {
           // Fill the gap with whitespace characters.
           while (lineNumber < scripts[i].lineOffset)
               appendChunk("\n");
           while (columnNumber < scripts[i].columnOffset - scriptOpenTag.length)
               appendChunk(" ");

           // Add script tag.
           appendChunk(scriptOpenTag);
           appendChunk(sources[i], scripts[i]);
           appendChunk(scriptCloseTag);
       }

       return content;
   }
}

WebInspector.ConcatenatedScriptsContentProvider.prototype.__proto__ = WebInspector.ContentProvider.prototype;


WebInspector.ResourceContentProvider = function(resource)
{
    this._mimeType = resource.type === WebInspector.Resource.Type.Script ? "text/javascript" : "text/html";
    this._resource = resource;
};

WebInspector.ResourceContentProvider.prototype = {
    requestContent: function(callback)
    {
        function didRequestContent(content)
        {
            callback(this._mimeType, content);
        }
        this._resource.requestContent(didRequestContent.bind(this));
    }
}

WebInspector.ResourceContentProvider.prototype.__proto__ = WebInspector.ContentProvider.prototype;
