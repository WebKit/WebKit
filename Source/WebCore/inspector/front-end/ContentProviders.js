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
        callback([]);
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

    searchInContent: function(query, callback)
    {
        callback([]);
    },

   _concatenateScriptsContent: function(scripts, sources)
   {
       var content = "";
       var lineNumber = 0;
       var columnNumber = 0;

       function appendChunk(chunk)
       {
           content += chunk;
           var lineEndings = chunk.lineEndings();
           var lineCount = lineEndings.length;
           if (lineCount === 1)
               columnNumber += chunk.length;
           else {
               lineNumber += lineCount - 1;
               columnNumber = lineEndings[lineCount - 1] - lineEndings[lineCount - 2] - 1;
           }
       }

       var scriptOpenTag = "<script>";
       var scriptCloseTag = "</script>";
       for (var i = 0; i < scripts.length; ++i) {
           if (lineNumber > scripts[i].lineOffset || (lineNumber === scripts[i].lineOffset && columnNumber > scripts[i].columnOffset - scriptOpenTag.length))
               continue;

           // Fill the gap with whitespace characters.
           while (lineNumber < scripts[i].lineOffset)
               appendChunk("\n");
           while (columnNumber < scripts[i].columnOffset - scriptOpenTag.length)
               appendChunk(" ");

           // Add script tag.
           appendChunk(scriptOpenTag);
           appendChunk(sources[i]);
           appendChunk(scriptCloseTag);
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
