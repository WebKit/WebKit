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
WebInspector.DebuggerScriptMapping = function()
{
    this._mappings = [];
  
    this._resourceMapping = new WebInspector.ResourceScriptMapping();
    this._mappings.push(this._resourceMapping);
    this._compilerMapping = new WebInspector.CompilerScriptMapping();
    this._mappings.push(this._compilerMapping);
    this._snippetMapping = WebInspector.scriptSnippetModel.scriptMapping;
    this._mappings.push(this._snippetMapping);

    WebInspector.debuggerModel.addEventListener(WebInspector.DebuggerModel.Events.ParsedScriptSource, this._parsedScriptSource, this);
    WebInspector.debuggerModel.addEventListener(WebInspector.DebuggerModel.Events.FailedToParseScriptSource, this._parsedScriptSource, this);
    WebInspector.debuggerModel.addEventListener(WebInspector.DebuggerModel.Events.GlobalObjectCleared, this._debuggerReset, this);
}

WebInspector.DebuggerScriptMapping.prototype = {
    /**
     * @return {Array.<WebInspector.UISourceCodeProvider>}
     */
    uiSourceCodeProviders: function()
    {
        return this._mappings;
    },

    /**
     * @param {WebInspector.Event} event
     */
    _parsedScriptSource: function(event)
    {
        var script = /** @type {WebInspector.Script} */ event.data;
        var mapping = this._mappingForScript(script);
        mapping.addScript(script);
    },

    /**
     * @param {WebInspector.Script} script
     * @return {WebInspector.SourceMapping}
     */
    _mappingForScript: function(script)
    {
        if (WebInspector.experimentsSettings.snippetsSupport.isEnabled()) {
            if (this._snippetMapping && this._snippetMapping.snippetIdForSourceURL(script.sourceURL))
                return this._snippetMapping;
        }

        if (WebInspector.settings.sourceMapsEnabled.get() && script.sourceMapURL) {
            if (this._compilerMapping.loadSourceMapForScript(script))
                return this._compilerMapping;
        }

        return this._resourceMapping;
    },

    _debuggerReset: function()
    {
        WebInspector.JavaScriptSource.javaScriptSourceForResource.clear();
        for (var i = 0; i < this._mappings.length; ++i)
            this._mappings[i].reset();
    }
}
