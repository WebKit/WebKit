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
 * @implements {WebInspector.ScriptSourceMapping}
 * @param {WebInspector.Workspace} workspace
 * @param {WebInspector.NetworkWorkspaceProvider} networkWorkspaceProvider
 */
WebInspector.CompilerScriptMapping = function(workspace, networkWorkspaceProvider)
{
    this._workspace = workspace;
    this._networkWorkspaceProvider = networkWorkspaceProvider;
    /** @type {Object.<string, WebInspector.SourceMapParser>} */
    this._sourceMapForSourceMapURL = {};
    /** @type {Object.<string, WebInspector.SourceMapParser>} */
    this._sourceMapForScriptId = {};
    this._scriptForSourceMap = new Map();
    /** @type {Object.<string, WebInspector.SourceMapParser>} */
    this._sourceMapForURL = {};
    this._workspace.addEventListener(WebInspector.Workspace.Events.ProjectWillReset, this._reset, this);
}

WebInspector.CompilerScriptMapping.prototype = {
    /**
     * @param {WebInspector.RawLocation} rawLocation
     * @return {WebInspector.UILocation}
     */
    rawLocationToUILocation: function(rawLocation)
    {
        var debuggerModelLocation = /** @type {WebInspector.DebuggerModel.Location} */ (rawLocation);
        var sourceMap = this._sourceMapForScriptId[debuggerModelLocation.scriptId];
        var lineNumber = debuggerModelLocation.lineNumber;
        var columnNumber = debuggerModelLocation.columnNumber || 0;
        var entry = sourceMap.findEntry(lineNumber, columnNumber);
        if (entry.length === 2)
            return null;
        var uiSourceCode = this._workspace.uiSourceCodeForURL(entry[2]);
        return new WebInspector.UILocation(uiSourceCode, entry[3], entry[4]);
    },

    /**
     * @param {WebInspector.UISourceCode} uiSourceCode
     * @param {number} lineNumber
     * @param {number} columnNumber
     * @return {WebInspector.DebuggerModel.Location}
     */
    uiLocationToRawLocation: function(uiSourceCode, lineNumber, columnNumber)
    {
        var sourceMap = this._sourceMapForURL[uiSourceCode.url];
        var entry = sourceMap.findEntryReversed(uiSourceCode.url, lineNumber);
        return WebInspector.debuggerModel.createRawLocation(this._scriptForSourceMap.get(sourceMap), entry[0], entry[1]);
    },

    /**
     * @param {WebInspector.Script} script
     */
    addScript: function(script)
    {
        var sourceMap = this.loadSourceMapForScript(script);

        if (this._scriptForSourceMap.get(sourceMap)) {
            this._sourceMapForScriptId[script.scriptId] = sourceMap;
            script.pushSourceMapping(this);
            return;
        }

        var sourceURLs = sourceMap.sources();
        for (var i = 0; i < sourceURLs.length; ++i) {
            var sourceURL = sourceURLs[i];
            if (this._workspace.uiSourceCodeForURL(sourceURL))
                continue;
            this._sourceMapForURL[sourceURL] = sourceMap;
            var sourceContent = sourceMap.sourceContent(sourceURL);
            var contentProvider;
            if (sourceContent)
                contentProvider = new WebInspector.StaticContentProvider(WebInspector.resourceTypes.Script, sourceContent);
            else
                contentProvider = new WebInspector.CompilerSourceMappingContentProvider(sourceURL);
            this._networkWorkspaceProvider.addFile(sourceURL, contentProvider, true);
            var uiSourceCode = this._workspace.uiSourceCodeForURL(sourceURL);
            uiSourceCode.setSourceMapping(this);
            uiSourceCode.isContentScript = script.isContentScript;
        }

        this._sourceMapForScriptId[script.scriptId] = sourceMap;
        this._scriptForSourceMap.put(sourceMap, script);
        script.pushSourceMapping(this);
    },

    /**
     * @param {WebInspector.Script} script
     * @return {WebInspector.SourceMapParser}
     */
    loadSourceMapForScript: function(script)
    {
        var sourceMapURL = WebInspector.SourceMapParser.prototype._canonicalizeURL(script.sourceMapURL, script.sourceURL);
        var sourceMap = this._sourceMapForSourceMapURL[sourceMapURL];
        if (sourceMap)
            return sourceMap;

        try {
            // FIXME: make sendRequest async.
            var response = InspectorFrontendHost.loadResourceSynchronously(sourceMapURL);
            if (response.slice(0, 3) === ")]}")
                response = response.substring(response.indexOf('\n'));
            var payload = /** @type {WebInspector.SourceMapPayload} */ (JSON.parse(response));
            sourceMap = new WebInspector.SourceMapParser(sourceMapURL, payload);
        } catch(e) {
            console.error(e.message);
            return null;
        }
        this._sourceMapForSourceMapURL[sourceMapURL] = sourceMap;
        return sourceMap;
    },

    _reset: function()
    {
        this._sourceMapForSourceMapURL = {};
        this._sourceMapForScriptId = {};
        this._scriptForSourceMap = new Map();
        this._sourceMapForURL = {};
    }
}

/**
 * @constructor
 */
WebInspector.SourceMapPayload = function()
{
    this.sections = [];
    this.mappings = "";
    this.sourceRoot = "";
    this.sources = [];
}

/**
 * Implements Source Map V3 consumer. See http://code.google.com/p/closure-compiler/wiki/SourceMaps
 * for format description.
 * @constructor
 * @param {string} sourceMappingURL
 * @param {WebInspector.SourceMapPayload} payload
 */
WebInspector.SourceMapParser = function(sourceMappingURL, payload)
{
    if (!WebInspector.SourceMapParser.prototype._base64Map) {
        const base64Digits = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
        WebInspector.SourceMapParser.prototype._base64Map = {};
        for (var i = 0; i < base64Digits.length; ++i)
            WebInspector.SourceMapParser.prototype._base64Map[base64Digits.charAt(i)] = i;
    }

    this._sourceMappingURL = sourceMappingURL;
    this._mappings = [];
    this._reverseMappingsBySourceURL = {};
    this._sourceContentByURL = {};
    this._parseMappingPayload(payload);
}

WebInspector.SourceMapParser.prototype = {
    /**
     * @return {Array.<string>}
     */
    sources: function()
    {
        var sources = [];
        for (var sourceURL in this._reverseMappingsBySourceURL)
            sources.push(sourceURL);
        return sources;
    },

    sourceContent: function(sourceURL)
    {
        return this._sourceContentByURL[sourceURL];
    },

    findEntry: function(lineNumber, columnNumber)
    {
        var first = 0;
        var count = this._mappings.length;
        while (count > 1) {
          var step = count >> 1;
          var middle = first + step;
          var mapping = this._mappings[middle];
          if (lineNumber < mapping[0] || (lineNumber == mapping[0] && columnNumber < mapping[1]))
              count = step;
          else {
              first = middle;
              count -= step;
          }
        }
        return this._mappings[first];
    },

    findEntryReversed: function(sourceURL, lineNumber)
    {
        var mappings = this._reverseMappingsBySourceURL[sourceURL];
        for ( ; lineNumber < mappings.length; ++lineNumber) {
            var mapping = mappings[lineNumber];
            if (mapping)
                return mapping;
        }
        return this._mappings[0];
    },

    _parseMappingPayload: function(mappingPayload)
    {
        if (mappingPayload.sections)
            this._parseSections(mappingPayload.sections);
        else
            this._parseMap(mappingPayload, 0, 0);
    },

    /**
     * @param {Array.<SourceMapV3.Section>} sections
     */
    _parseSections: function(sections)
    {
        for (var i = 0; i < sections.length; ++i) {
            var section = sections[i];
            this._parseMap(section.map, section.offset.line, section.offset.column)
        }
    },

    /**
     * @param {SourceMapV3} map
     * @param {number} lineNumber
     * @param {number} columnNumber
     */
    _parseMap: function(map, lineNumber, columnNumber)
    {
        var sourceIndex = 0;
        var sourceLineNumber = 0;
        var sourceColumnNumber = 0;
        var nameIndex = 0;

        var sources = [];
        for (var i = 0; i < map.sources.length; ++i) {
            var sourceURL = map.sources[i];
            if (map.sourceRoot)
                sourceURL = map.sourceRoot + "/" + sourceURL;
            var url = this._canonicalizeURL(sourceURL, this._sourceMappingURL);
            sources.push(url);
            if (!this._reverseMappingsBySourceURL[url])
                this._reverseMappingsBySourceURL[url] = [];
            if (map.sourcesContent && map.sourcesContent[i])
                this._sourceContentByURL[url] = map.sourcesContent[i];
        }

        var stringCharIterator = new WebInspector.SourceMapParser.StringCharIterator(map.mappings);
        var sourceURL = sources[sourceIndex];
        var reverseMappings = this._reverseMappingsBySourceURL[sourceURL];

        while (true) {
            if (stringCharIterator.peek() === ",")
                stringCharIterator.next();
            else {
                while (stringCharIterator.peek() === ";") {
                    lineNumber += 1;
                    columnNumber = 0;
                    stringCharIterator.next();
                }
                if (!stringCharIterator.hasNext())
                    break;
            }

            columnNumber += this._decodeVLQ(stringCharIterator);
            if (this._isSeparator(stringCharIterator.peek())) {
                this._mappings.push([lineNumber, columnNumber]);
                continue;
            }

            var sourceIndexDelta = this._decodeVLQ(stringCharIterator);
            if (sourceIndexDelta) {
                sourceIndex += sourceIndexDelta;
                sourceURL = sources[sourceIndex];
                reverseMappings = this._reverseMappingsBySourceURL[sourceURL];
            }
            sourceLineNumber += this._decodeVLQ(stringCharIterator);
            sourceColumnNumber += this._decodeVLQ(stringCharIterator);
            if (!this._isSeparator(stringCharIterator.peek()))
                nameIndex += this._decodeVLQ(stringCharIterator);

            this._mappings.push([lineNumber, columnNumber, sourceURL, sourceLineNumber, sourceColumnNumber]);
            if (!reverseMappings[sourceLineNumber])
                reverseMappings[sourceLineNumber] = [lineNumber, columnNumber];
        }
    },

    _isSeparator: function(char)
    {
        return char === "," || char === ";";
    },

    _decodeVLQ: function(stringCharIterator)
    {
        // Read unsigned value.
        var result = 0;
        var shift = 0;
        do {
            var digit = this._base64Map[stringCharIterator.next()];
            result += (digit & this._VLQ_BASE_MASK) << shift;
            shift += this._VLQ_BASE_SHIFT;
        } while (digit & this._VLQ_CONTINUATION_MASK);

        // Fix the sign.
        var negative = result & 1;
        result >>= 1;
        return negative ? -result : result;
    },

    _canonicalizeURL: function(url, baseURL)
    {
        if (!url || !baseURL || url.asParsedURL() || url.substring(0, 5) === "data:")
            return url;

        var base = baseURL.asParsedURL();
        if (!base)
            return url;

        var baseHost = base.scheme + "://" + base.host + (base.port ? ":" + base.port : "");
        if (url[0] === "/")
            return baseHost + url;
        return baseHost + base.folderPathComponents + "/" + url;
    },

    _VLQ_BASE_SHIFT: 5,
    _VLQ_BASE_MASK: (1 << 5) - 1,
    _VLQ_CONTINUATION_MASK: 1 << 5
}

/**
 * @constructor
 */
WebInspector.SourceMapParser.StringCharIterator = function(string)
{
    this._string = string;
    this._position = 0;
}

WebInspector.SourceMapParser.StringCharIterator.prototype = {
    next: function()
    {
        return this._string.charAt(this._position++);
    },

    peek: function()
    {
        return this._string.charAt(this._position);
    },

    hasNext: function()
    {
        return this._position < this._string.length;
    }
}
