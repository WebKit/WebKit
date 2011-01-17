/*
 * Copyright (C) 2007, 2008 Apple Inc.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

WebInspector.SourceView = function(resource)
{
    WebInspector.ResourceView.call(this, resource);

    this.element.addStyleClass("source");

    var contentProvider = new WebInspector.SourceFrameContentProviderForResource(resource);
    var canEditScripts = WebInspector.panels.scripts.canEditScripts() && resource.type === WebInspector.Resource.Type.Script;
    this.sourceFrame = new WebInspector.SourceFrame(this.element, contentProvider, resource.url, canEditScripts);
}

WebInspector.SourceView.prototype = {
    show: function(parentElement)
    {
        WebInspector.View.prototype.show.call(this, parentElement);
        this.sourceFrame.visible = true;
    },

    hide: function()
    {
        this.sourceFrame.visible = false;
        this.sourceFrame.clearLineHighlight();
        WebInspector.View.prototype.hide.call(this);
        this._currentSearchResultIndex = -1;
    },

    resize: function()
    {
        this.sourceFrame.resize();
    },

    get scrollTop()
    {
        return this.sourceFrame.scrollTop;
    },

    set scrollTop(scrollTop)
    {
        this.sourceFrame.scrollTop = scrollTop;
    },

    hasContent: function()
    {
        return true;
    },

    // The rest of the methods in this prototype need to be generic enough to work with a ScriptView.
    // The ScriptView prototype pulls these methods into it's prototype to avoid duplicate code.

    searchCanceled: function()
    {
        this._currentSearchResultIndex = -1;
        this._searchResults = [];
        this.sourceFrame.clearMarkedRange();
        this.sourceFrame.cancelFindSearchMatches();
    },

    performSearch: function(query, finishedCallback)
    {
        // Call searchCanceled since it will reset everything we need before doing a new search.
        this.searchCanceled();

        function didFindSearchMatches(searchResults)
        {
            this._searchResults = searchResults;
            if (this._searchResults)
                finishedCallback(this, this._searchResults.length);
        }
        this.sourceFrame.findSearchMatches(query, didFindSearchMatches.bind(this));
    },

    jumpToFirstSearchResult: function()
    {
        if (!this._searchResults || !this._searchResults.length)
            return;
        this._currentSearchResultIndex = 0;
        this._jumpToSearchResult(this._currentSearchResultIndex);
    },

    jumpToLastSearchResult: function()
    {
        if (!this._searchResults || !this._searchResults.length)
            return;
        this._currentSearchResultIndex = (this._searchResults.length - 1);
        this._jumpToSearchResult(this._currentSearchResultIndex);
    },

    jumpToNextSearchResult: function()
    {
        if (!this._searchResults || !this._searchResults.length)
            return;
        if (++this._currentSearchResultIndex >= this._searchResults.length)
            this._currentSearchResultIndex = 0;
        this._jumpToSearchResult(this._currentSearchResultIndex);
    },

    jumpToPreviousSearchResult: function()
    {
        if (!this._searchResults || !this._searchResults.length)
            return;
        if (--this._currentSearchResultIndex < 0)
            this._currentSearchResultIndex = (this._searchResults.length - 1);
        this._jumpToSearchResult(this._currentSearchResultIndex);
    },

    showingFirstSearchResult: function()
    {
        return (this._currentSearchResultIndex === 0);
    },

    showingLastSearchResult: function()
    {
        return (this._searchResults && this._currentSearchResultIndex === (this._searchResults.length - 1));
    },

    revealLine: function(lineNumber)
    {
        this.sourceFrame.revealLine(lineNumber);
    },

    highlightLine: function(lineNumber)
    {
        this.sourceFrame.highlightLine(lineNumber);
    },

    addMessage: function(msg)
    {
        this.sourceFrame.addMessage(msg);
    },

    clearMessages: function()
    {
        this.sourceFrame.clearMessages();
    },

    _jumpToSearchResult: function(index)
    {
        var foundRange = this._searchResults[index];
        if (!foundRange)
            return;

        this.sourceFrame.markAndRevealRange(foundRange);
    }
}

WebInspector.SourceView.prototype.__proto__ = WebInspector.ResourceView.prototype;


WebInspector.SourceFrameContentProviderForResource = function(resource)
{
    WebInspector.SourceFrameContentProvider.call(this);
    this._resource = resource;
}

//This is a map from resource.type to mime types
//found in WebInspector.SourceTokenizer.Registry.
WebInspector.SourceFrameContentProviderForResource.DefaultMIMETypeForResourceType = {
 0: "text/html",
 1: "text/css",
 4: "text/javascript"
}

WebInspector.SourceFrameContentProviderForResource.prototype = {
    requestContent: function(callback)
    {
        function contentLoaded(content)
        {
            var mimeType = WebInspector.SourceFrameContentProviderForResource.DefaultMIMETypeForResourceType[this._resource.type] || this._resource.mimeType;
            callback(mimeType, content);
        }
        this._resource.requestContent(contentLoaded.bind(this));
    },

    scripts: function()
    {
        return WebInspector.debuggerModel.scriptsForURL(this._resource.url);
    }
}

WebInspector.SourceFrameContentProviderForResource.prototype.__proto__ = WebInspector.SourceFrameContentProvider.prototype;
