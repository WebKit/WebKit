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
    this._element.textContent = "Search:";

    this._searchInputElement = this._element.createChild("input");
    this._searchInputElement.id = "search";
    this._searchInputElement.type = "search";
    this._searchInputElement.incremental = true;
    this._searchInputElement.results = 0;

    this._searchNavigationNextElement = this._element.createChild("div", "toolbar-search-navigation toolbar-search-navigation-next hidden");
    this._searchNavigationNextElement.addEventListener("mousedown", this._onNextButtonSearch.bind(this), false); 
    this._searchNavigationNextElement.title = WebInspector.UIString("Search Next");

    this._searchNavigationPrevElement = this._element.createChild("div", "toolbar-search-navigation toolbar-search-navigation-prev hidden");
    this._searchNavigationPrevElement.addEventListener("mousedown", this._onPrevButtonSearch.bind(this), false);
    this._searchNavigationPrevElement.title = WebInspector.UIString("Search Previous");

    this._matchesElement = this._element.createChild("span", "search-results-matches");

    this._searchInputElement.addEventListener("search", this._onSearch.bind(this), false); // when the search is emptied
    this._searchInputElement.addEventListener("mousedown", this._onSearchFieldManualFocus.bind(this), false); // when the search field is manually selected
    this._searchInputElement.addEventListener("keydown", this._onKeyDown.bind(this), true);

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
            this._updateSearchMatchesCountAndCurrentMatchIndex(WebInspector.inspectorView.currentPanel().currentQuery && matches);
    },

    updateCurrentMatchIndex: function(currentMatchIndex, panel)
    {
        if (panel === WebInspector.inspectorView.currentPanel())
            this._updateSearchMatchesCountAndCurrentMatchIndex(panel.currentSearchMatches, currentMatchIndex);
    },

    updateSearchLabel: function()
    {
        var panelName = WebInspector.inspectorView.currentPanel() && WebInspector.inspectorView.currentPanel().toolbarItemLabel;
        if (!panelName)
            return;
        var newLabel = WebInspector.UIString("Search %s", panelName);
        this._searchInputElement.setAttribute("placeholder", newLabel);
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
        this.updateSearchLabel();

        if (!this._currentQuery)
            return;

        var panel = WebInspector.inspectorView.currentPanel();
        if (panel.performSearch) {
            function performPanelSearch()
            {
                this._updateSearchMatchesCountAndCurrentMatchIndex();

                panel.currentQuery = this._currentQuery;
                panel.performSearch(this._currentQuery);
            }

            // Perform the search on a timeout so the panel switches fast.
            setTimeout(performPanelSearch.bind(this), 0);
        } else {
            // Update to show Not found for panels that can't be searched.
            this._updateSearchMatchesCountAndCurrentMatchIndex();
        }
    },

    _updateSearchNavigationButtonState: function(enabled)
    {
        if (enabled) {
            this._searchNavigationPrevElement.removeStyleClass("hidden");
            this._searchNavigationNextElement.removeStyleClass("hidden");
        } else {
            this._searchNavigationPrevElement.addStyleClass("hidden");
            this._searchNavigationNextElement.addStyleClass("hidden");
        }
    },

    /**
     * @param {?number=} matches
     * @param {number=} currentMatchIndex
     */
    _updateSearchMatchesCountAndCurrentMatchIndex: function(matches, currentMatchIndex)
    {
        if (matches == null) {
            this._matchesElement.addStyleClass("hidden");
            // Make Search Nav key non-accessible when there is no active search.
            this._updateSearchNavigationButtonState(false); 
            return;
        }

        if (matches) {
            if (matches === 1) {
                if (currentMatchIndex === 0)
                    var matchesString = WebInspector.UIString("1 of 1 match");
                else
                    var matchesString = WebInspector.UIString("1 match");
            } else {
                if (typeof currentMatchIndex === "number")
                    var matchesString = WebInspector.UIString("%d of %d matches", currentMatchIndex + 1, matches);
                else
                    var matchesString = WebInspector.UIString("%d matches", matches);
                // Make search nav key accessible when there are more than 1 search results found.    
                this._updateSearchNavigationButtonState(true);
            }
        } else {
            var matchesString = WebInspector.UIString("Not Found");
            // Make search nav key non-accessible when there is no match found.
            this._updateSearchNavigationButtonState(false);
        }

        this._matchesElement.removeStyleClass("hidden");
        this._matchesElement.textContent = matchesString;
        WebInspector.toolbar.resize();
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

        if (!isEnterKey(event))
            return false;

        // Select all of the text so the user can easily type an entirely new query.
        event.target.select();

        // Only call performSearch if the Enter key was pressed. Otherwise the search
        // performance is poor because of searching on every key. The search field has
        // the incremental attribute set, so we still get incremental searches.
        this._onSearch(event);

        // Call preventDefault since this was the Enter key. This prevents a "search" event
        // from firing for key down. This stops performSearch from being called twice in a row.
        event.preventDefault();
    },

    _onSearch: function(event)
    {
        var forceSearch = event.keyIdentifier === "Enter";
        this._performSearch(event.target.value, forceSearch, event.shiftKey, false);
    },

    _onNextButtonSearch: function(event)
    {
        // Simulate next search on search-navigation-button click.
        this._performSearch(this._searchInputElement.value, true, false, false);
    },

    _onPrevButtonSearch: function(event)
    {
        // Simulate previous search on search-navigation-button click.
        this._performSearch(this._searchInputElement.value, true, true, false);
    },

    /**
     * @param {boolean=} forceSearch
     * @param {boolean=} isBackwardSearch
     * @param {boolean=} repeatSearch
     */
    _performSearch: function(query, forceSearch, isBackwardSearch, repeatSearch)
    {
        var isShortSearch = (query.length < 3);

        // Clear a leftover short search flag due to a non-conflicting forced search.
        if (isShortSearch && this._shortSearchWasForcedByKeyEvent && this._currentQuery !== query)
            delete this._shortSearchWasForcedByKeyEvent;

        // Indicate this was a forced search on a short query.
        if (isShortSearch && forceSearch)
            this._shortSearchWasForcedByKeyEvent = true;

        if (!query || !query.length || (!forceSearch && isShortSearch)) {
            // Prevent clobbering a short search forced by the user.
            if (this._shortSearchWasForcedByKeyEvent) {
                delete this._shortSearchWasForcedByKeyEvent;
                return;
            }

            delete this._currentQuery;

            for (var panelName in WebInspector.panels) {
                var panel = WebInspector.panels[panelName];
                var hadCurrentQuery = !!panel.currentQuery;
                delete panel.currentQuery;
                if (hadCurrentQuery && panel.searchCanceled)
                    panel.searchCanceled();
            }

            this._updateSearchMatchesCountAndCurrentMatchIndex();

            return;
        }

        var currentPanel = WebInspector.inspectorView.currentPanel();
        if (!repeatSearch && query === currentPanel.currentQuery && currentPanel.currentQuery === this._currentQuery) {
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

        this._currentQuery = query;

        this._updateSearchMatchesCountAndCurrentMatchIndex();

        if (!currentPanel.performSearch)
            return;

        currentPanel.currentQuery = query;
        currentPanel.performSearch(query);
    }
}

/**
 * @type {?WebInspector.SearchController}
 */
WebInspector.searchController = null;
