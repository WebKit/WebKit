/*
 * Copyright (C) 2013, 2015 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

WebInspector.SearchSidebarPanel = class SearchSidebarPanel extends WebInspector.NavigationSidebarPanel
{
    constructor(contentBrowser)
    {
        super("search", WebInspector.UIString("Search"), true, true);

        this.contentBrowser = contentBrowser;

        var searchElement = document.createElement("div");
        searchElement.classList.add("search-bar");
        this.element.appendChild(searchElement);

        this._inputElement = document.createElement("input");
        this._inputElement.type = "search";
        this._inputElement.spellcheck = false;
        this._inputElement.addEventListener("search", this._searchFieldChanged.bind(this));
        this._inputElement.addEventListener("input", this._searchFieldInput.bind(this));
        this._inputElement.setAttribute("results", 5);
        this._inputElement.setAttribute("autosave", "inspector-search-autosave");
        this._inputElement.setAttribute("placeholder", WebInspector.UIString("Search Resource Content"));
        searchElement.appendChild(this._inputElement);

        this.filterBar.placeholder = WebInspector.UIString("Filter Search Results");

        this._searchQuerySetting = new WebInspector.Setting("search-sidebar-query", "");
        this._inputElement.value = this._searchQuerySetting.value;

        WebInspector.Frame.addEventListener(WebInspector.Frame.Event.MainResourceDidChange, this._mainResourceDidChange, this);

        this.contentTreeOutline.addEventListener(WebInspector.TreeOutline.Event.SelectionDidChange, this._treeSelectionDidChange, this);
    }

    // Public

    closed()
    {
        super.closed();

        WebInspector.Frame.removeEventListener(null, null, this);
    }

    focusSearchField()
    {
        this.show();

        this._inputElement.select();
    }

    performSearch(searchQuery)
    {
        // Before performing a new search, clear the old search.
        this.contentTreeOutline.removeChildren();
        this.contentBrowser.contentViewContainer.closeAllContentViews();

        this._inputElement.value = searchQuery;
        this._searchQuerySetting.value = searchQuery;

        this.hideEmptyContentPlaceholder();

        searchQuery = searchQuery.trim();
        if (!searchQuery.length)
            return;

        // FIXME: Provide UI to toggle regex and case sensitive searches.
        var isCaseSensitive = false;
        var isRegex = false;

        var updateEmptyContentPlaceholderTimeout = null;

        function updateEmptyContentPlaceholderSoon()
        {
            if (updateEmptyContentPlaceholderTimeout)
                return;
            updateEmptyContentPlaceholderTimeout = setTimeout(updateEmptyContentPlaceholder.bind(this), 100);
        }

        function updateEmptyContentPlaceholder()
        {
            if (updateEmptyContentPlaceholderTimeout) {
                clearTimeout(updateEmptyContentPlaceholderTimeout);
                updateEmptyContentPlaceholderTimeout = null;
            }

            this.updateEmptyContentPlaceholder(WebInspector.UIString("No Search Results"));
        }

        function forEachMatch(searchQuery, lineContent, callback)
        {
            var lineMatch;
            var searchRegex = new RegExp(searchQuery.escapeForRegExp(), "gi");
            while ((searchRegex.lastIndex < lineContent.length) && (lineMatch = searchRegex.exec(lineContent)))
                callback(lineMatch, searchRegex.lastIndex);
        }

        function resourcesCallback(error, result)
        {
            updateEmptyContentPlaceholderSoon.call(this);

            if (error)
                return;

            function resourceCallback(frameId, url, error, resourceMatches)
            {
                updateEmptyContentPlaceholderSoon.call(this);

                if (error || !resourceMatches || !resourceMatches.length)
                    return;

                var frame = WebInspector.frameResourceManager.frameForIdentifier(frameId);
                if (!frame)
                    return;

                var resource = frame.url === url ? frame.mainResource : frame.resourceForURL(url);
                if (!resource)
                    return;

                var resourceTreeElement = this._searchTreeElementForResource(resource);

                for (var i = 0; i < resourceMatches.length; ++i) {
                    var match = resourceMatches[i];
                    forEachMatch(searchQuery, match.lineContent, function(lineMatch, lastIndex) {
                        var matchObject = new WebInspector.SourceCodeSearchMatchObject(resource, match.lineContent, searchQuery, new WebInspector.TextRange(match.lineNumber, lineMatch.index, match.lineNumber, lastIndex));
                        var matchTreeElement = new WebInspector.SearchResultTreeElement(matchObject);
                        resourceTreeElement.appendChild(matchTreeElement);
                        if (!this.contentTreeOutline.selectedTreeElement)
                            matchTreeElement.revealAndSelect(false, true);
                    }.bind(this));
                }

                updateEmptyContentPlaceholder.call(this);
            }

            for (var i = 0; i < result.length; ++i) {
                var searchResult = result[i];
                if (!searchResult.url || !searchResult.frameId)
                    continue;

                // COMPATIBILITY (iOS 9): Page.searchInResources did not have the optional requestId parameter.
                PageAgent.searchInResource(searchResult.frameId, searchResult.url, searchQuery, isCaseSensitive, isRegex, searchResult.requestId, resourceCallback.bind(this, searchResult.frameId, searchResult.url));
            }
        }

        function searchScripts(scriptsToSearch)
        {
            updateEmptyContentPlaceholderSoon.call(this);

            if (!scriptsToSearch.length)
                return;

            function scriptCallback(script, error, scriptMatches)
            {
                updateEmptyContentPlaceholderSoon.call(this);

                if (error || !scriptMatches || !scriptMatches.length)
                    return;

                var scriptTreeElement = this._searchTreeElementForScript(script);

                for (var i = 0; i < scriptMatches.length; ++i) {
                    var match = scriptMatches[i];
                    forEachMatch(searchQuery, match.lineContent, function(lineMatch, lastIndex) {
                        var matchObject = new WebInspector.SourceCodeSearchMatchObject(script, match.lineContent, searchQuery, new WebInspector.TextRange(match.lineNumber, lineMatch.index, match.lineNumber, lastIndex));
                        var matchTreeElement = new WebInspector.SearchResultTreeElement(matchObject);
                        scriptTreeElement.appendChild(matchTreeElement);
                        if (!this.contentTreeOutline.selectedTreeElement)
                            matchTreeElement.revealAndSelect(false, true);
                    }.bind(this));
                }

                updateEmptyContentPlaceholder.call(this);
            }

            for (var script of scriptsToSearch)
                DebuggerAgent.searchInContent(script.id, searchQuery, isCaseSensitive, isRegex, scriptCallback.bind(this, script));
        }

        function domCallback(error, searchId, resultsCount)
        {
            updateEmptyContentPlaceholderSoon.call(this);

            if (error || !resultsCount)
                return;

            console.assert(searchId);

            this._domSearchIdentifier = searchId;

            function domSearchResults(error, nodeIds)
            {
                updateEmptyContentPlaceholderSoon.call(this);

                if (error)
                    return;

                for (var i = 0; i < nodeIds.length; ++i) {
                    // If someone started a new search, then return early and stop showing seach results from the old query.
                    if (this._domSearchIdentifier !== searchId)
                        return;

                    var domNode = WebInspector.domTreeManager.nodeForId(nodeIds[i]);
                    if (!domNode || !domNode.ownerDocument)
                        continue;

                    // We do not display the document node when the search query is "/". We don't have anything to display in the content view for it.
                    if (domNode.nodeType() === Node.DOCUMENT_NODE)
                        continue;

                    // FIXME: This should use a frame to do resourceForURL, but DOMAgent does not provide a frameId.
                    var resource = WebInspector.frameResourceManager.resourceForURL(domNode.ownerDocument.documentURL);
                    if (!resource)
                        continue;

                    var resourceTreeElement = this._searchTreeElementForResource(resource);
                    var domNodeTitle = WebInspector.DOMSearchMatchObject.titleForDOMNode(domNode);

                    // Textual matches.
                    var didFindTextualMatch = false;
                    forEachMatch(searchQuery, domNodeTitle, function(lineMatch, lastIndex) {
                        var matchObject = new WebInspector.DOMSearchMatchObject(resource, domNode, domNodeTitle, searchQuery, new WebInspector.TextRange(0, lineMatch.index, 0, lastIndex));
                        var matchTreeElement = new WebInspector.SearchResultTreeElement(matchObject);
                        resourceTreeElement.appendChild(matchTreeElement);
                        if (!this.contentTreeOutline.selectedTreeElement)
                            matchTreeElement.revealAndSelect(false, true);
                        didFindTextualMatch = true;
                    }.bind(this));

                    // Non-textual matches are CSS Selector or XPath matches. In such cases, display the node entirely highlighted.
                    if (!didFindTextualMatch) {
                        var matchObject = new WebInspector.DOMSearchMatchObject(resource, domNode, domNodeTitle, domNodeTitle, new WebInspector.TextRange(0, 0, 0, domNodeTitle.length));
                        var matchTreeElement = new WebInspector.SearchResultTreeElement(matchObject);
                        resourceTreeElement.appendChild(matchTreeElement);
                        if (!this.contentTreeOutline.selectedTreeElement)
                            matchTreeElement.revealAndSelect(false, true);
                    }

                    updateEmptyContentPlaceholder.call(this);
                }
            }

            DOMAgent.getSearchResults(searchId, 0, resultsCount, domSearchResults.bind(this));
        }

        if (window.DOMAgent)
            WebInspector.domTreeManager.requestDocument(function(){});

        if (window.PageAgent)
            PageAgent.searchInResources(searchQuery, isCaseSensitive, isRegex, resourcesCallback.bind(this));

        setTimeout(searchScripts.bind(this, WebInspector.debuggerManager.searchableScripts), 0);

        if (window.DOMAgent) {
            if (this._domSearchIdentifier) {
                DOMAgent.discardSearchResults(this._domSearchIdentifier);
                this._domSearchIdentifier = undefined;
            }

            DOMAgent.performSearch(searchQuery, domCallback.bind(this));
        }

        // FIXME: Resource search should work in JSContext inspection.
        // <https://webkit.org/b/131252> Web Inspector: JSContext inspection Resource search does not work
        if (!window.DOMAgent && !window.PageAgent)
            updateEmptyContentPlaceholderSoon.call(this);
    }

    // Private

    _searchFieldChanged(event)
    {
        this.performSearch(event.target.value);
    }

    _searchFieldInput(event)
    {
        // If the search field is cleared, immediately clear the search results tree outline.
        if (!event.target.value.length)
            this.performSearch("");
    }

    _searchTreeElementForResource(resource)
    {
        var resourceTreeElement = this.contentTreeOutline.getCachedTreeElement(resource);
        if (!resourceTreeElement) {
            resourceTreeElement = new WebInspector.ResourceTreeElement(resource);
            resourceTreeElement.hasChildren = true;
            resourceTreeElement.expand();

            this.contentTreeOutline.appendChild(resourceTreeElement);
        }

        return resourceTreeElement;
    }

    _searchTreeElementForScript(script)
    {
        var scriptTreeElement = this.contentTreeOutline.getCachedTreeElement(script);
        if (!scriptTreeElement) {
            scriptTreeElement = new WebInspector.ScriptTreeElement(script);
            scriptTreeElement.hasChildren = true;
            scriptTreeElement.expand();

            this.contentTreeOutline.appendChild(scriptTreeElement);
        }

        return scriptTreeElement;
    }

    _mainResourceDidChange(event)
    {
        if (!event.target.isMainFrame())
            return;

        if (this._delayedSearchTimeout) {
            clearTimeout(this._delayedSearchTimeout);
            this._delayedSearchTimeout = undefined;
        }

        this.contentTreeOutline.removeChildren();
        this.contentBrowser.contentViewContainer.closeAllContentViews();

        if (this.visible)
            this.focusSearchField();
    }

    _treeSelectionDidChange(event)
    {
        let treeElement = event.data.selectedElement;
        if (!treeElement || treeElement instanceof WebInspector.FolderTreeElement)
            return;

        if (treeElement instanceof WebInspector.ResourceTreeElement || treeElement instanceof WebInspector.ScriptTreeElement) {
            WebInspector.showRepresentedObject(treeElement.representedObject);
            return;
        }

        console.assert(treeElement instanceof WebInspector.SearchResultTreeElement);
        if (!(treeElement instanceof WebInspector.SearchResultTreeElement))
            return;

        if (treeElement.representedObject instanceof WebInspector.DOMSearchMatchObject)
            WebInspector.showMainFrameDOMTree(treeElement.representedObject.domNode);
        else if (treeElement.representedObject instanceof WebInspector.SourceCodeSearchMatchObject)
            WebInspector.showOriginalOrFormattedSourceCodeTextRange(treeElement.representedObject.sourceCodeTextRange);
    }
};
