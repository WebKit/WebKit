/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 * 1. Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY GOOGLE INC. AND ITS CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL GOOGLE INC.
 * OR ITS CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
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
WebInspector.AdvancedSearchController = function()
{
    this._shortcut = WebInspector.AdvancedSearchController.createShortcut();
    this._searchId = 0;
    
    WebInspector.settings.advancedSearchConfig = WebInspector.settings.createSetting("advancedSearchConfig", new WebInspector.SearchConfig("", true, false));
}

WebInspector.AdvancedSearchController.createShortcut = function()
{
    return WebInspector.KeyboardShortcut.makeDescriptor("f", WebInspector.KeyboardShortcut.Modifiers.CtrlOrMeta | WebInspector.KeyboardShortcut.Modifiers.Shift);
}

WebInspector.AdvancedSearchController.prototype = {
    /**
     * @param {Event} event
     */
    handleShortcut: function(event)
    {
        if (WebInspector.KeyboardShortcut.makeKeyFromEvent(event) === this._shortcut.key) {
            this.show();
            event.handled = true;
        }
    },

    /**
     * @param {WebInspector.SearchScope} searchScope
     */
    registerSearchScope: function(searchScope)
    {
        // FIXME: implement multiple search scopes.
        this._searchScope = searchScope;
    },

    show: function()
    {
        if (!this._searchView)
            this._searchView = new WebInspector.SearchView(this);
        
        if (this._searchView.visible)
            this._searchView.focus();
        else
            WebInspector.showViewInDrawer(this._searchView);
    },
    
    /**
     * @param {number} searchId
     * @param {Object} searchResult
     */
    _onSearchResult: function(searchId, searchResult)
    {
        if (searchId !== this._searchId)
            return;

        if (!this._searchResultsPane) 
            this._searchResultsPane = this._currentSearchScope.createSearchResultsPane(this._searchConfig);        
        this._searchView.resultsPane = this._searchResultsPane; 
        this._searchResultsPane.addSearchResult(searchResult);
    },
    
    /**
     * @param {number} searchId
     */
    _onSearchFinished: function(searchId)
    {
        if (searchId !== this._searchId)
            return;

        if (!this._searchResultsPane)
            this._searchView.nothingFound();
        
        this._searchView.searchFinished();
    },
    
    /**
     * @param {WebInspector.SearchConfig} searchConfig
     */
    startSearch: function(searchConfig)
    {
        this.stopSearch();

        this._searchConfig = searchConfig;
        // FIXME: this._currentSearchScope should be initialized based on searchConfig
        this._currentSearchScope = this._searchScope;

        this._searchView.searchStarted();
        this._currentSearchScope.performSearch(searchConfig, this._onSearchResult.bind(this, this._searchId), this._onSearchFinished.bind(this, this._searchId));
    },
    
    stopSearch: function()
    {
        ++this._searchId;
        delete this._searchResultsPane;
        if (this._currentSearchScope)
            this._currentSearchScope.stopSearch();
    }
}

/**
 * @constructor
 * @extends {WebInspector.View}
 * @param {WebInspector.AdvancedSearchController} controller
 */
WebInspector.SearchView = function(controller)
{
    WebInspector.View.call(this);
    
    this._controller = controller;

    this.element = document.createElement("div");
    this.element.className = "search-view";

    this._searchPanelElement = this.element.createChild("div");
    this._searchPanelElement.tabIndex = 0;
    this._searchPanelElement.className = "search-panel";
    this._searchPanelElement.addEventListener("keydown", this._onKeyDown.bind(this), false);
    
    this._searchResultsElement = this.element.createChild("div");
    this._searchResultsElement.className = "search-results";
    
    this._search = this._searchPanelElement.createChild("input");
    this._search.setAttribute("type", "search");
    this._search.addStyleClass("search-config-search");
    this._search.setAttribute("results", "0");
    this._search.setAttribute("size", 20);

    this._ignoreCaseLabel = this._searchPanelElement.createChild("label");
    this._ignoreCaseLabel.addStyleClass("search-config-label");
    this._ignoreCaseCheckbox = this._ignoreCaseLabel.createChild("input");
    this._ignoreCaseCheckbox.setAttribute("type", "checkbox");
    this._ignoreCaseCheckbox.addStyleClass("search-config-checkbox");
    this._ignoreCaseLabel.appendChild(document.createTextNode(WebInspector.UIString("Ignore case")));
    

    this._regexLabel = this._searchPanelElement.createChild("label");
    this._regexLabel.addStyleClass("search-config-label");
    this._regexCheckbox = this._regexLabel.createChild("input");
    this._regexCheckbox.setAttribute("type", "checkbox");
    this._regexCheckbox.addStyleClass("search-config-checkbox");
    this._regexLabel.appendChild(document.createTextNode(WebInspector.UIString("Regular expression")));
    
    this._load();
}

// Number of recent search queries to store.
WebInspector.SearchView.maxQueriesCount = 20;

WebInspector.SearchView.prototype = {
    /**
     * @type {WebInspector.SearchConfig}
     */
    get searchConfig()
    {
        var searchConfig = {};
        searchConfig.query = this._search.value;
        searchConfig.ignoreCase = this._ignoreCaseCheckbox.checked;
        searchConfig.isRegex = this._regexCheckbox.checked;
        return searchConfig;
    },
    
    /**
     * @type {WebInspector.SearchResultsPane}
     */
    set resultsPane(resultsPane)
    {
        this._searchResultsElement.removeChildren();
        this._searchResultsElement.appendChild(resultsPane.element);
    },
    
    searchStarted: function()
    {
        // FIXME: This needs better UI.
        var searchingView = new WebInspector.EmptyView(WebInspector.UIString("Searching..."))
        this._searchResultsElement.removeChildren();
        searchingView.show(this._searchResultsElement);
    },

    nothingFound: function()
    {
        // FIXME: This needs better UI.
        var notFoundView = new WebInspector.EmptyView(WebInspector.UIString("Nothing found"))
        this._searchResultsElement.removeChildren();
        notFoundView.show(this._searchResultsElement);
    },

    searchFinished: function()
    {
        // FIXME: add message to drawer status bar
    },

    focus: function()
    {
        WebInspector.currentFocusElement = this._search;
        this._search.select();
    },

    wasShown: function()
    {
        this.focus();
    },

    wasHidden: function()
    {
        this._controller.stopSearch();
    },

    /**
     * @param {Event} event
     */
    _onKeyDown: function(event)
    {
        if (event.keyCode === WebInspector.KeyboardShortcut.Keys.Enter.code)
            this._onAction();
    },
    
    _save: function()
    {
        var searchConfig = new WebInspector.SearchConfig(this.searchConfig.query, this.searchConfig.ignoreCase, this.searchConfig.isRegex); 
        WebInspector.settings.advancedSearchConfig.set(searchConfig);
    },
    
    _load: function()
    {
        var searchConfig = WebInspector.settings.advancedSearchConfig.get();
        this._search.value = searchConfig.query;
        this._ignoreCaseCheckbox.checked = searchConfig.ignoreCase;
        this._regexCheckbox.checked = searchConfig.isRegex;
    },
    
    _onAction: function()
    {
        this._save();
        this._controller.startSearch(this.searchConfig);
    }
}

WebInspector.SearchView.prototype.__proto__ = WebInspector.View.prototype;

/**
 * @constructor
 * @param {string} query
 * @param {boolean} ignoreCase
 * @param {boolean} isRegex
 */
WebInspector.SearchConfig = function(query, ignoreCase, isRegex)
{
    this.query = query;
    this.ignoreCase = ignoreCase;
    this.isRegex = isRegex;
}

/**
 * @interface
 */
WebInspector.SearchScope = function()
{
}

WebInspector.SearchScope.prototype = {
    /**
     * @param {WebInspector.SearchConfig} searchConfig
     */
    performSearch: function(searchConfig, searchResultCallback, searchFinishedCallback) { },

    stopSearch: function() { },
    
    /**
     * @return WebInspector.SearchResultsPane}
     */
    createSearchResultsPane: function() { }
}

/**
 * @constructor
 * @param {WebInspector.SearchConfig} searchConfig
 */
WebInspector.SearchResultsPane = function(searchConfig)
{
    this._searchConfig = searchConfig;
    this.element = document.createElement("div");
}

WebInspector.SearchResultsPane.prototype = {
    /**
     * @type {WebInspector.SearchConfig}
     */
    get searchConfig()
    {
        return this._searchConfig;
    },

    /**
     * @param {Object} searchResult
     */
    addSearchResult: function(searchResult) { }
}

/**
 * @constructor
 * @extends {WebInspector.SearchResultsPane} 
 * @param {WebInspector.SearchConfig} searchConfig
 */
WebInspector.FileBasedSearchResultsPane = function(searchConfig)
{
    WebInspector.SearchResultsPane.call(this, searchConfig);
    
    this._searchResults = [];

    this.element.id ="search-results-pane-file-based";
    
    this._treeOutlineElement = document.createElement("ol");
    this._treeOutlineElement.className = "outline-disclosure";
    this.element.appendChild(this._treeOutlineElement);
    this._treeOutline = new TreeOutline(this._treeOutlineElement);
}

WebInspector.FileBasedSearchResultsPane.prototype = {
    /**
     * @param {Object} file
     * @param {number} lineNumber
     * @return {Element}
     */
    createAnchor: function(file, lineNumber) { },
    
    /**
     * @param {Object} file
     * @return {string}
     */
    fileName: function(file) { },
    
    /**
     * @param {Object} searchResult
     */
    addSearchResult: function(searchResult)
    {
        this._searchResults.push(searchResult);
        var file = searchResult.file;
        var fileName = this.fileName(file);
        var searchMatches = searchResult.searchMatches;
            
        // Expand first file with matches only.
        var fileTreeElement = this._addFileTreeElement(fileName, searchMatches.length, this._searchResults.length === 1);
        
        var regexObject = createSearchRegex(this._searchConfig.query, !this._searchConfig.ignoreCase, this._searchConfig.isRegex);
        for (var i = 0; i < searchMatches.length; i++) {
            var lineNumber = searchMatches[i].lineNumber;
            
            var anchor = this.createAnchor(file, lineNumber);
            
            var numberString = numberToStringWithSpacesPadding(lineNumber + 1, 4);
            var lineNumberSpan = document.createElement("span");
            lineNumberSpan.addStyleClass("webkit-line-number");
            lineNumberSpan.addStyleClass("search-match-line-number");
            lineNumberSpan.textContent = numberString;
            anchor.appendChild(lineNumberSpan);
            
            var contentSpan = this._createContentSpan(searchMatches[i].lineContent, regexObject);
            anchor.appendChild(contentSpan);
            
            var searchMatchElement = new TreeElement("", null, false);
            fileTreeElement.appendChild(searchMatchElement);
            searchMatchElement.listItemElement.className = "search-match";
            searchMatchElement.listItemElement.appendChild(anchor);
        }
    },
        
    /**
     * @param {string} fileName
     * @param {number} searchMatchesCount
     * @param {boolean} expanded
     */
    _addFileTreeElement: function(fileName, searchMatchesCount, expanded)
    {
        var fileTreeElement = new TreeElement("", null, true);
        fileTreeElement.expanded = expanded;
        fileTreeElement.toggleOnClick = true;
        fileTreeElement.selectable = false;

        this._treeOutline.appendChild(fileTreeElement);
        fileTreeElement.listItemElement.addStyleClass("search-result");

        var fileNameSpan = document.createElement("span");
        fileNameSpan.className = "search-result-file-name";
        fileNameSpan.textContent = fileName;
        fileTreeElement.listItemElement.appendChild(fileNameSpan);

        var matchesCountSpan = document.createElement("span");
        matchesCountSpan.className = "search-result-matches-count";
        if (searchMatchesCount === 1)
            matchesCountSpan.textContent = WebInspector.UIString("(%d match)", searchMatchesCount);
        else
            matchesCountSpan.textContent = WebInspector.UIString("(%d matches)", searchMatchesCount);
        
        fileTreeElement.listItemElement.appendChild(matchesCountSpan);
        
        return fileTreeElement; 
    },

    /**
     * @param {string} lineContent
     * @param {RegExp} regexObject
     */
    _createContentSpan: function(lineContent, regexObject)
    {
        var contentSpan = document.createElement("span");
        contentSpan.className = "search-match-content";
        contentSpan.textContent = lineContent;
        
        regexObject.lastIndex = 0;
        var match = regexObject.exec(lineContent);
        var offset = 0;
        var matchRanges = [];
        while (match) {
            matchRanges.push({ offset: match.index, length: match[0].length });
            match = regexObject.exec(lineContent);
        }
        highlightRangesWithStyleClass(contentSpan, matchRanges, "highlighted-match");
        
        return contentSpan;
    }
}

WebInspector.FileBasedSearchResultsPane.prototype.__proto__ = WebInspector.SearchResultsPane.prototype;

/**
 * @constructor
 * @param {Object} file
 * @param {Array.<Object>} searchMatches
 */
WebInspector.FileBasedSearchResultsPane.SearchResult = function(file, searchMatches) {
    this.file = file;
    this.searchMatches = searchMatches;
}
