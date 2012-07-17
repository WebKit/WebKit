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

    this._searchInputElement = this._searchControlElement.createChild("input", "search-replace");
    this._searchInputElement.id = "search-input-field";

    this._matchesElement = this._searchControlElement.createChild("label", "search-results-matches");
    this._matchesElement.setAttribute("for", "search-input-field");

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

    this._replaceElement = this._element.createChild("span");

    this._replaceCheckboxElement = this._replaceElement.createChild("input");
    this._replaceCheckboxElement.type = "checkbox";
    this._replaceCheckboxElement.id = "search-replace-trigger";
    this._replaceCheckboxElement.tabIndex = -1;
    this._replaceCheckboxElement.addEventListener("click", this._toggleReplaceVisibility.bind(this), false);

    this._replaceLabelElement = this._replaceElement.createChild("label");
    this._replaceLabelElement.textContent = WebInspector.UIString("Replace");
    this._replaceLabelElement.setAttribute("for", "search-replace-trigger");

    this._replaceDetailsElement = this._replaceElement.createChild("span", "hidden");

    this._replaceInputElement = this._replaceDetailsElement.createChild("input", "search-replace toolbar-replace-control");
    this._replaceInputElement.addEventListener("keydown", this._onKeyDown.bind(this), true);

    this._replaceButtonElement = this._replaceDetailsElement.createChild("button");
    this._replaceButtonElement.textContent = WebInspector.UIString("Replace");
    this._replaceButtonElement.addEventListener("click", this._replace.bind(this), false);

    this._skipButtonElement = this._replaceDetailsElement.createChild("button");
    this._skipButtonElement.textContent = WebInspector.UIString("Skip");
    this._skipButtonElement.addEventListener("click", this._onNextButtonSearch.bind(this), false);

    this._replaceAllButtonElement = this._replaceDetailsElement.createChild("button");
    this._replaceAllButtonElement.textContent = WebInspector.UIString("Replace All");
    this._replaceAllButtonElement.addEventListener("click", this._replaceAll.bind(this), false);

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
        if (!this._searchIsVisible)
            return;
        delete this._searchIsVisible;
        this._performSearch("", false, false, false);
        WebInspector.inspectorView.setFooterElement(null);
        this._replaceCheckboxElement.checked = false;
        this._toggleReplaceVisibility();
    },

    disableSearchUntilExplicitAction: function(event)
    {
        this._performSearch("", false, false, false);
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
                    this.showSearchField();
                    event.consume(true);
                    return true;
                }
                break;

            case "F3":
                if (!isMac) {
                    this.showSearchField();
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

    _updateSearchNavigationButtonState: function(enabled)
    {
        var panel = WebInspector.inspectorView.currentPanel();
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

    showSearchField: function()
    {
        WebInspector.inspectorView.setFooterElement(this._element);
        this._updateReplaceVisibility();
        this._searchInputElement.focus();
        this._searchInputElement.select();
        this._searchIsVisible = true;
    },

    _updateReplaceVisibility: function()
    {
        var panel = WebInspector.inspectorView.currentPanel();
        if (WebInspector.experimentsSettings.searchReplace.isEnabled() && panel.canSearchAndReplace())
            this._replaceElement.removeStyleClass("hidden");
        else
            this._replaceElement.addStyleClass("hidden");
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

        if (isEnterKey(event)) {
            if (event.target === this._searchInputElement)
                this._performSearch(event.target.value, true, event.shiftKey, true);
            else if (event.target === this._replaceInputElement)
                this._replace();
        }
    },

    _onInput: function(event)
    {
        this._performSearch(event.target.value, false, false, true);
    },

    _onNextButtonSearch: function(event)
    {
        // Simulate next search on search-navigation-button click.
        this._performSearch(this._searchInputElement.value, true, false, true);
        this._searchInputElement.focus();
    },

    _onPrevButtonSearch: function(event)
    {
        // Simulate previous search on search-navigation-button click.
        this._performSearch(this._searchInputElement.value, true, true, true);
        this._searchInputElement.focus();
    },

    /**
     * @param {string} query
     * @param {boolean} forceSearch
     * @param {boolean} isBackwardSearch
     * @param {boolean} loop
     */
    _performSearch: function(query, forceSearch, isBackwardSearch, loop)
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
                if (!isBackwardSearch)
                    currentPanel.jumpToNextSearchResult(loop);
                else if (isBackwardSearch)
                    currentPanel.jumpToPreviousSearchResult(loop);
            }
            return;
        }

        if (!forceSearch && query.length < 3 && !this._currentQuery)
            return;

        this._currentQuery = query;

        currentPanel.currentQuery = query;
        currentPanel.performSearch(query, loop);
    },

    _toggleReplaceVisibility: function()
    {
        if (this._replaceCheckboxElement.checked) {
            this._replaceDetailsElement.removeStyleClass("hidden");
            this._replaceInputElement.focus();
        } else
            this._replaceDetailsElement.addStyleClass("hidden");
    },

    _replace: function()
    {
        var currentPanel = WebInspector.inspectorView.currentPanel();
        currentPanel.replaceSelectionWith(this._replaceInputElement.value);
        var query = this._currentQuery;
        delete this._currentQuery;
        this._performSearch(query, true, false, false);
    },

    _replaceAll: function()
    {
        var currentPanel = WebInspector.inspectorView.currentPanel();
        currentPanel.replaceAllWith(this._currentQuery, this._replaceInputElement.value);
    }
}

/**
 * @type {?WebInspector.SearchController}
 */
WebInspector.searchController = null;
