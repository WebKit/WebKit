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
 * @implements {WebInspector.SearchScope}
 */
WebInspector.ScriptsSearchScope = function()
{
    // FIXME: Add title once it is used by search controller.
    WebInspector.SearchScope.call(this)
}

WebInspector.ScriptsSearchScope.prototype = {
    /**
     * @param {WebInspector.SearchConfig} searchConfig
     * @param {function(Object)} searchResultCallback
     * @param {function()} searchFinishedCallback
     */
    performSearch: function(searchConfig, searchResultCallback, searchFinishedCallback)
    {
        var callbacksLeft = 0;

        function maybeSearchFinished()
        {
            if (callbacksLeft === 0)
                searchFinishedCallback();                
        }
        
        function searchCallbackWrapper(uiSourceCode, searchMatches)
        {
            if (searchMatches.length) {
                var searchResult = {file: uiSourceCode, searchMatches: searchMatches};
                searchResultCallback(searchResult);
            }
            --callbacksLeft;
            maybeSearchFinished();
        }
        
        var uiSourceCodes = this._sortedUISourceCodes();
        // FIXME: Enable support for counting matches for incremental search.
        // FIXME: Enable support for bounding search results/matches number to keep inspector responsive.
        for (var i = 0; i < uiSourceCodes.length; i++) {
            var uiSourceCode = uiSourceCodes[i];
            // FIXME: Add setting to search in content scripts as well.
            if (!uiSourceCode.isContentScript) {
                // Increase callbacksLeft first because searchInContent call could be synchronous.
                callbacksLeft++;
                // FIXME: We should not request next searchInContent unless previous one is already finished.  
                uiSourceCode.searchInContent(searchConfig.query, searchCallbackWrapper.bind(this, uiSourceCode));
            }
        }
        maybeSearchFinished();
    },

    stopSearch: function()
    {
        // FIXME: Implement search so that it could be stopped.
    },

    /**
     * @param {WebInspector.SearchConfig} searchConfig
     */
    createSearchResultsPane: function(searchConfig)
    {
        return new WebInspector.ScriptsSearchResultsPane(searchConfig);
    },

    /**
     * @return {Array.<WebInspector.UISourceCode>}
     */
    _sortedUISourceCodes: function()
    {
        function filterOutAnonymous(uiSourceCode)
        {
            return !!uiSourceCode.url;
        }
        
        function comparator(a, b)
        {
            return a.url.localeCompare(b.url);   
        }
        
        var uiSourceCodes = WebInspector.debuggerPresentationModel.uiSourceCodes();
        
        uiSourceCodes = uiSourceCodes.filter(filterOutAnonymous);
        uiSourceCodes.sort(comparator);
        
        return uiSourceCodes;
    }
}

WebInspector.ScriptsSearchScope.prototype.__proto__ = WebInspector.SearchScope.prototype;

/**
 * @constructor
 * @extends {WebInspector.FileBasedSearchResultsPane}
 * @param {WebInspector.SearchConfig} searchConfig
 */
WebInspector.ScriptsSearchResultsPane = function(searchConfig)
{
    WebInspector.FileBasedSearchResultsPane.call(this, searchConfig)
}

WebInspector.ScriptsSearchResultsPane.prototype = {
    /**
     * @param {Object} file
     * @param {number} lineNumber
     */
    createAnchor: function(file, lineNumber)
    {
        var uiSourceCode = file;
        
        var anchor = WebInspector.linkifyURLAsNode(uiSourceCode.url, "");
        anchor.setAttribute("preferred_panel", "scripts");
        anchor.uiSourceCode = uiSourceCode;
        anchor.lineNumber = lineNumber;
        anchor.removeChildren();
        
        return anchor;
    },
    
    /**
     * @param {Object} file
     * @return {string}
     */
    fileName: function(file)
    {
        var uiSourceCode = file;
        return uiSourceCode.url;
    },
}

WebInspector.ScriptsSearchResultsPane.prototype.__proto__ = WebInspector.FileBasedSearchResultsPane.prototype;
