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
 * @implements {WebInspector.ContentProvider}
 */
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
    },

    searchInContent: function(query, callback)
    {
        this._script.searchInContent(query, callback);
    }
}

WebInspector.ScriptContentProvider.prototype.__proto__ = WebInspector.ContentProvider.prototype;

/**
 * @constructor
 * @implements {WebInspector.ContentProvider}
 */
WebInspector.ConcatenatedScriptsContentProvider = function(scripts)
{
    this._mimeType = "text/html";
    this._scripts = scripts;
};

WebInspector.ConcatenatedScriptsContentProvider.scriptOpenTag = "<script>";
WebInspector.ConcatenatedScriptsContentProvider.scriptCloseTag = "</script>";

WebInspector.ConcatenatedScriptsContentProvider.prototype = {
    _sortedScripts: function()
    {
        if (this._sortedScriptsArray)
            return this._sortedScriptsArray;

        this._sortedScriptsArray = [];
        
        var scripts = this._scripts.slice();
        scripts.sort(function(x, y) { return x.lineOffset - y.lineOffset || x.columnOffset - y.columnOffset; });
        
        var scriptOpenTagLength = WebInspector.ConcatenatedScriptsContentProvider.scriptOpenTag.length;
        var scriptCloseTagLength = WebInspector.ConcatenatedScriptsContentProvider.scriptCloseTag.length;
        
        this._sortedScriptsArray.push(scripts[0]);
        for (var i = 1; i < scripts.length; ++i) {
            var previousScript = this._sortedScriptsArray[this._sortedScriptsArray.length - 1];
            
            var lineNumber = previousScript.endLine;
            var columnNumber = previousScript.endColumn + scriptCloseTagLength + scriptOpenTagLength;
            
            if (lineNumber < scripts[i].lineOffset || (lineNumber === scripts[i].lineOffset && columnNumber <= scripts[i].columnOffset))
                this._sortedScriptsArray.push(scripts[i]);
        }
        return this._sortedScriptsArray;
    },

    requestContent: function(callback)
    {
        var scripts = this._sortedScripts();
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

    searchInContent: function(query, callback)
    {
        var results = {};
        var scripts = this._sortedScripts();
        var scriptsLeft = scripts.length;

        function maybeCallback()
        {
            if (scriptsLeft)
                return;

            var result = [];
            for (var i = 0; i < scripts.length; ++i)
                result = result.concat(results[scripts[i].scriptId]);
            callback(result);
        }

        function searchCallback(script, searchMatches)
        {
            results[script.scriptId] = [];
            for (var i = 0; i < searchMatches.length; ++i) {
                var searchMatch = {};
                searchMatch.lineNumber = searchMatches[i].lineNumber + script.lineOffset;
                searchMatch.lineContent = searchMatches[i].lineContent;
                results[script.scriptId].push(searchMatch);
            }
            scriptsLeft--;
            maybeCallback.call(this);
        }

        maybeCallback();
        for (var i = 0; i < scripts.length; ++i)
            scripts[i].searchInContent(query, searchCallback.bind(this, scripts[i]));
    },

    _concatenateScriptsContent: function(scripts, sources)
    {
        var content = "";
        var lineNumber = 0;
        var columnNumber = 0;

        var scriptOpenTag = WebInspector.ConcatenatedScriptsContentProvider.scriptOpenTag;
        var scriptCloseTag = WebInspector.ConcatenatedScriptsContentProvider.scriptCloseTag;
        for (var i = 0; i < scripts.length; ++i) {
            // Fill the gap with whitespace characters.
            for (var newLinesCount = scripts[i].lineOffset - lineNumber; newLinesCount > 0; --newLinesCount) {
                columnNumber = 0;
                content += "\n";
            }
            for (var spacesCount = scripts[i].columnOffset - columnNumber - scriptOpenTag.length; spacesCount > 0; --spacesCount)
                content += " ";

            // Add script tag.
            content += scriptOpenTag;
            content += sources[i];
            content += scriptCloseTag;
            lineNumber = scripts[i].endLine;
            columnNumber = scripts[i].endColumn + scriptCloseTag.length;
        }

        return content;
    }
}

WebInspector.ConcatenatedScriptsContentProvider.prototype.__proto__ = WebInspector.ContentProvider.prototype;

/**
 * @constructor
 * @implements {WebInspector.ContentProvider}
 */
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
    },

    searchInContent: function(query, callback)
    {
        this._resource.searchInContent(query, callback);
    }
}

WebInspector.ResourceContentProvider.prototype.__proto__ = WebInspector.ContentProvider.prototype;

/**
 * @constructor
 * @implements {WebInspector.ContentProvider}
 */
WebInspector.CompilerSourceMappingContentProvider = function(sourceURL, compilerSourceMappingProvider)
{
    this._mimeType = "text/javascript";
    this._sourceURL = sourceURL;
    this._compilerSourceMappingProvider = compilerSourceMappingProvider;
};

WebInspector.CompilerSourceMappingContentProvider.prototype = {
    requestContent: function(callback)
    {
        function didLoadSourceCode(sourceCode)
        {
            callback(this._mimeType, sourceCode);
        }
        this._compilerSourceMappingProvider.loadSourceCode(this._sourceURL, didLoadSourceCode.bind(this));
    },

    searchInContent: function(query, callback)
    {
        callback([]);
    }
}

WebInspector.CompilerSourceMappingContentProvider.prototype.__proto__ = WebInspector.ContentProvider.prototype;

/**
 * @constructor
 * @implements {WebInspector.ContentProvider}
 */
WebInspector.StaticContentProvider = function(mimeType, content)
{
    this._mimeType = mimeType;
    this._content = content;
};

WebInspector.StaticContentProvider.prototype = {
    requestContent: function(callback)
    {
        callback(this._mimeType, this._content);
    },

    searchInContent: function(query, callback)
    {
        callback([]);
    }
}

WebInspector.StaticContentProvider.prototype.__proto__ = WebInspector.ContentProvider.prototype;
