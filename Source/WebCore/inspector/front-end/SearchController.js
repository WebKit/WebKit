/*
 * Copyright (C) 2006, 2007, 2008 Apple Inc.  All rights reserved.
 * Copyright (C) 2007 Matt Lilek (pewtermoose@gmail.com).
 * Copyright (C) 2009 Joseph Pecoraro
 * Copyright (C) 2011 Google Inc. All rights reserved.
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

/**
 * @constructor
 */
WebInspector.SearchController = function()
{
    this._element = document.createElement("div");
    this._element.className = "toolbar-search";

    var labelElement = this._element.createChild("span");
    labelElement.textContent = WebInspector.UIString("Find");

    this._searchControlElement = this._element.createChild("div", "toolbar-search-control");

    this._searchInputElement = this._searchControlElement.createChild("input");
    this._searchInputElement.id = "search";

    this._matchesElement = this._searchControlElement.createChild("label", "search-results-matches");
    this._matchesElement.setAttribute("for", "search");

    var searchNavigationElement = this._searchControlElement.createChild("div", "toolbar-search-navigation-controls");
    this._searchNavigationPrevElement = searchNavigationElement.createChild("div", "toolbar-search-navigation toolbar-search-navigation-prev");
    this._searchNavigationPrevElement.addEventListener("click", this._onPrevButtonSearch.bind(this), false);
    this._searchNavigationPrevElement.title = WebInspector.UIString("Search Previous");

    this._searchNavigationNextElement = searchNavigationElement.createChild("div", "toolbar-search-navigation toolbar-search-navigation-next"); 
    this._searchNavigationNextElement.addEventListener("click", this._onNextButtonSearch.bind(this), false);
    this._searchNavigationNextElement.title = WebInspector.UIString("Search Next");

    this._searchInputElement.addEventListener("mousedown", this._onSearchFieldManualFocus.bind(this), false); // when the search field is manually selected
    this._searchInputElement.addEventListener("keydown", this._onKeyDown.bind(this), true);
    this._searchInputElement.addEventListener("input", this._onInput.bind(this), false);

    var closeButtonElement = this._element.createChild("span", "drawer-header-close-button");
    closeButtonElement.textContent = WebInspector.UIString("\u00D7");
    closeButtonElement.addEventListener("click", this.cancelSearch.bind(this), false);
}

WebInspector.SearchController.prototype = {
    updateSearchMatchesCount: function(matches, panel)
    {
        if (!panel)
            panel = WebInspector.inspectorView.currentPanel();

        panel.currentSearchMatches = matches;

        if (panel === WebInspector.inspectorView.currentPanel())
            this._updateSearchMatchesCountAndCurrentMatchIndex(WebInspector.inspectorView.currentPanel().currentQuery ? matches : 0, -1);
    },

    updateCurrentMatchIndex: function(currentMatchIndex, panel)
    {
        if (panel === WebInspector.inspectorView.currentPanel())
            this._updateSearchMatchesCountAndCurrentMatchIndex(panel.currentSearchMatches, currentMatchIndex);
    },

    cancelSearch: function()
    {
        this._searchInputElement.value = "";
        this._performSearch("");
        WebInspector.inspectorView.setFooterElement(null);
    },

    disableSearchUntilExplicitAction: function(event)
    {
        this._performSearch("");
    },

    /**
     * @param {Event} event
     * @return {boolean}
     */
    handleShortcut: function(event)
    {
        var isMac = WebInspector.isMac();

        switch (event.keyIdentifier) {
            case "U+0046": // F key
                if (isMac)
                    var isFindKey = event.metaKey && !event.ctrlKey && !event.altKey && !event.shiftKey;
                else
                    var isFindKey = event.ctrlKey && !event.metaKey && !event.altKey && !event.shiftKey;

                if (isFindKey) {
                    this.focusSearchField();
                    event.consume(true);
                    return true;
                }
                break;


            case "F3":
                if (!isMac) {
                    this.focusSearchField();
                    event.consume();
                }
                break;

            case "U+0047": // G key
                var currentPanel = WebInspector.inspectorView.currentPanel();

                if (isMac && event.metaKey && !event.ctrlKey && !event.altKey) {
                    if (event.shiftKey) {
                        if (currentPanel.jumpToPreviousSearchResult)
                            currentPanel.jumpToPreviousSearchResult();
                    } else if (currentPanel.jumpToNextSearchResult)
                        currentPanel.jumpToNextSearchResult();
                    event.consume(true);
                    return true;
                }
                break;
        }
        return false;
    },

    activePanelChanged: function()
    {
        if (!this._currentQuery)
            return;

        var panel = WebInspector.inspectorView.currentPanel();
        if (panel.performSearch) {
            function performPanelSearch()
            {
                this._updateSearchMatchesCountAndCurrentMatchIndex(0, -1);

                panel.currentQuery = this._currentQuery;
                panel.performSearch(this._currentQuery);
            }

            // Perform the search on a timeout so the panel switches fast.
            setTimeout(performPanelSearch.bind(this), 0);
        } else {
            // Update to show Not found for panels that can't be searched.
            this._updateSearchMatchesCountAndCurrentMatchIndex(0, -1);
        }
    },

    _updateSearchNavigationButtonState: function(enabled)
    {
        if (enabled) {
            this._searchNavigationPrevElement.addStyleClass("enabled");
            this._searchNavigationNextElement.addStyleClass("enabled");
        } else {
            this._searchNavigationPrevElement.removeStyleClass("enabled");
            this._searchNavigationNextElement.removeStyleClass("enabled");
        }
    },

    /**
     * @param {number} matches
     * @param {number} currentMatchIndex
     */
    _updateSearchMatchesCountAndCurrentMatchIndex: function(matches, currentMatchIndex)
    {
        if (matches === 0 || currentMatchIndex >= 0)
            this._matchesElement.textContent = WebInspector.UIString("%d of %d", currentMatchIndex + 1, matches);
        this._updateSearchNavigationButtonState(matches > 0);
    },

    focusSearchField: function()
    {
        WebInspector.inspectorView.setFooterElement(this._element);
        this._searchInputElement.focus();
        this._searchInputElement.select();
    },

    _onSearchFieldManualFocus: function(event)
    {
        WebInspector.setCurrentFocusElement(event.target);
    },

    _onKeyDown: function(event)
    {
        // Escape Key will clear the field and clear the search results
        if (event.keyCode === WebInspector.KeyboardShortcut.Keys.Esc.code) {
            event.consume(true);
            this.cancelSearch();
            WebInspector.setCurrentFocusElement(WebInspector.previousFocusElement());
            if (WebInspector.currentFocusElement() === event.target)
                WebInspector.currentFocusElement().select();
            return false;
        }

        if (isEnterKey(event))
            this._performSearch(event.target.value, true, event.shiftKey);
    },

    _onInput: function(event)
    {
        this._performSearch(event.target.value, false, false);
    },

    _onNextButtonSearch: function(event)
    {
        // Simulate next search on search-navigation-button click.
        this._performSearch(this._searchInputElement.value, true, false);
        this._searchInputElement.focus();
    },

    _onPrevButtonSearch: function(event)
    {
        // Simulate previous search on search-navigation-button click.
        this._performSearch(this._searchInputElement.value, true, true);
        this._searchInputElement.focus();
    },

    /**
     * @param {boolean=} forceSearch
     * @param {boolean=} isBackwardSearch
     */
    _performSearch: function(query, forceSearch, isBackwardSearch)
    {
        if (!query || !query.length) {
            delete this._currentQuery;

            for (var panelName in WebInspector.panels) {
                var panel = WebInspector.panels[panelName];
                var hadCurrentQuery = !!panel.currentQuery;
                delete panel.currentQuery;
                if (hadCurrentQuery && panel.searchCanceled)
                    panel.searchCanceled();
            }
            this._updateSearchMatchesCountAndCurrentMatchIndex(0, -1);
            return;
        }

        var currentPanel = WebInspector.inspectorView.currentPanel();
        if (query === currentPanel.currentQuery && currentPanel.currentQuery === this._currentQuery) {
            // When this is the same query and a forced search, jump to the next
            // search result for a good user experience.
            if (forceSearch) {
                if (!isBackwardSearch && currentPanel.jumpToNextSearchResult)
                    currentPanel.jumpToNextSearchResult();
                else if (isBackwardSearch && currentPanel.jumpToPreviousSearchResult)
                    currentPanel.jumpToPreviousSearchResult();
            }
            return;
        }

        if (!forceSearch && query.length < 3 && !this._currentQuery)
            return;

        this._currentQuery = query;

        if (!currentPanel.performSearch) {
            this._updateSearchMatchesCountAndCurrentMatchIndex(0, -1);
            return;
        }

        currentPanel.currentQuery = query;
        currentPanel.performSearch(query);
    }
}

/**
 * @type {?WebInspector.SearchController}
 */
WebInspector.searchController = null;
