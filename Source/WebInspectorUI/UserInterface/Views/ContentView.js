/*
 * Copyright (C) 2013 Apple Inc. All rights reserved.
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

WebInspector.ContentView = function(representedObject, extraArguments)
{
    if (this.constructor === WebInspector.ContentView) {
        // When instantiated directly return an instance of a type-based concrete subclass.

        console.assert(representedObject);

        if (representedObject instanceof WebInspector.Frame)
            return new WebInspector.ResourceClusterContentView(representedObject.mainResource, extraArguments);

        if (representedObject instanceof WebInspector.Resource)
            return new WebInspector.ResourceClusterContentView(representedObject, extraArguments);

        if (representedObject instanceof WebInspector.Script)
            return new WebInspector.ScriptContentView(representedObject, extraArguments);

        if (representedObject instanceof WebInspector.TimelineRecording)
            return new WebInspector.TimelineRecordingContentView(representedObject, extraArguments);

        if (representedObject instanceof WebInspector.Timeline) {
            var timelineType = representedObject.type;
            if (timelineType === WebInspector.TimelineRecord.Type.Network)
                return new WebInspector.NetworkTimelineView(representedObject, extraArguments);

            if (timelineType === WebInspector.TimelineRecord.Type.Layout)
                return new WebInspector.LayoutTimelineView(representedObject, extraArguments);

            if (timelineType === WebInspector.TimelineRecord.Type.Script)
                return new WebInspector.ScriptTimelineView(representedObject, extraArguments);

            if (timelineType === WebInspector.TimelineRecord.Type.RenderingFrame)
                return new WebInspector.RenderingFrameTimelineView(representedObject, extraArguments);
        }

        if (representedObject instanceof WebInspector.Breakpoint) {
            if (representedObject.sourceCodeLocation)
                return new WebInspector.ContentView(representedObject.sourceCodeLocation.displaySourceCode, extraArguments);
        }

        if (representedObject instanceof WebInspector.DOMStorageObject)
            return new WebInspector.DOMStorageContentView(representedObject, extraArguments);

        if (representedObject instanceof WebInspector.CookieStorageObject)
            return new WebInspector.CookieStorageContentView(representedObject, extraArguments);

        if (representedObject instanceof WebInspector.DatabaseTableObject)
            return new WebInspector.DatabaseTableContentView(representedObject, extraArguments);

        if (representedObject instanceof WebInspector.DatabaseObject)
            return new WebInspector.DatabaseContentView(representedObject, extraArguments);

        if (representedObject instanceof WebInspector.IndexedDatabaseObjectStore)
            return new WebInspector.IndexedDatabaseObjectStoreContentView(representedObject, extraArguments);

        if (representedObject instanceof WebInspector.IndexedDatabaseObjectStoreIndex)
            return new WebInspector.IndexedDatabaseObjectStoreContentView(representedObject, extraArguments);

        if (representedObject instanceof WebInspector.ApplicationCacheFrame)
            return new WebInspector.ApplicationCacheFrameContentView(representedObject, extraArguments);

        if (representedObject instanceof WebInspector.DOMTree)
            return new WebInspector.FrameDOMTreeContentView(representedObject, extraArguments);

        if (representedObject instanceof WebInspector.DOMSearchMatchObject) {
            var resultView = new WebInspector.FrameDOMTreeContentView(WebInspector.frameResourceManager.mainFrame.domTree, extraArguments);
            resultView.restoreFromCookie({nodeToSelect: representedObject.domNode});
            return resultView;
        }

        if (representedObject instanceof WebInspector.SourceCodeSearchMatchObject) {
            var resultView;
            if (representedObject.sourceCode instanceof WebInspector.Resource)
                resultView = new WebInspector.ResourceClusterContentView(representedObject.sourceCode, extraArguments);
            else if (representedObject.sourceCode instanceof WebInspector.Script)
                resultView = new WebInspector.ScriptContentView(representedObject.sourceCode, extraArguments);
            else
                console.error("Unknown SourceCode", representedObject.sourceCode);

            var textRangeToSelect = representedObject.sourceCodeTextRange.formattedTextRange;
            var startPosition = textRangeToSelect.startPosition();
            resultView.restoreFromCookie({lineNumber: startPosition.lineNumber, columnNumber: startPosition.columnNumber});

            return resultView;
        }

        if (representedObject instanceof WebInspector.LogObject)
            return new WebInspector.LogContentView(representedObject, extraArguments);

        if (representedObject instanceof WebInspector.ContentFlow)
            return new WebInspector.ContentFlowDOMTreeContentView(representedObject, extraArguments);

        if (typeof representedObject === "string" || representedObject instanceof String)
            return new WebInspector.TextContentView(representedObject, extraArguments);

        console.assert(!WebInspector.ContentView.isViewable(representedObject));

        throw "Can't make a ContentView for an unknown representedObject.";
    }

    // Concrete object instantiation.
    console.assert(this.constructor !== WebInspector.ContentView && this instanceof WebInspector.ContentView);
    console.assert(!representedObject || WebInspector.ContentView.isViewable(representedObject));

    // FIXME: Convert this to a WebInspector.Object subclass, and call super().
    // WebInspector.Object.call(this);

    this._representedObject = representedObject;

    this._element = document.createElement("div");
    this._element.classList.add("content-view");

    this._parentContainer = null;
};

// FIXME: Move to a WebInspector.Object subclass and we can remove this.
WebInspector.Object.deprecatedAddConstructorFunctions(WebInspector.ContentView);

WebInspector.ContentView.isViewable = function(representedObject)
{
    if (representedObject instanceof WebInspector.Frame)
        return true;
    if (representedObject instanceof WebInspector.Resource)
        return true;
    if (representedObject instanceof WebInspector.Script)
        return true;
    if (representedObject instanceof WebInspector.TimelineRecording)
        return true;
    if (representedObject instanceof WebInspector.Timeline)
        return true;
    if (representedObject instanceof WebInspector.Breakpoint)
        return representedObject.sourceCodeLocation;
    if (representedObject instanceof WebInspector.DOMStorageObject)
        return true;
    if (representedObject instanceof WebInspector.CookieStorageObject)
        return true;
    if (representedObject instanceof WebInspector.DatabaseTableObject)
        return true;
    if (representedObject instanceof WebInspector.DatabaseObject)
        return true;
    if (representedObject instanceof WebInspector.IndexedDatabaseObjectStore)
        return true;
    if (representedObject instanceof WebInspector.IndexedDatabaseObjectStoreIndex)
        return true;
    if (representedObject instanceof WebInspector.ApplicationCacheFrame)
        return true;
    if (representedObject instanceof WebInspector.DOMTree)
        return true;
    if (representedObject instanceof WebInspector.DOMSearchMatchObject)
        return true;
    if (representedObject instanceof WebInspector.SourceCodeSearchMatchObject)
        return true;
    if (representedObject instanceof WebInspector.LogObject)
        return true;
    if (representedObject instanceof WebInspector.ContentFlow)
        return true;
    if (typeof representedObject === "string" || representedObject instanceof String)
        return true;
    return false;
};

WebInspector.ContentView.Event = {
    SelectionPathComponentsDidChange: "content-view-selection-path-components-did-change",
    SupplementalRepresentedObjectsDidChange: "content-view-supplemental-represented-objects-did-change",
    NumberOfSearchResultsDidChange: "content-view-number-of-search-results-did-change",
    NavigationItemsDidChange: "content-view-navigation-items-did-change"
};

WebInspector.ContentView.prototype = {
    constructor: WebInspector.ContentView,

    // Public

    get representedObject()
    {
        return this._representedObject;
    },

    get navigationItems()
    {
        // Navigation items that will be displayed by the ContentBrowser instance,
        // meant to be subclassed. Implemented by subclasses.
        return [];
    },

    get element()
    {
        return this._element;
    },

    get parentContainer()
    {
        return this._parentContainer;
    },

    get visible()
    {
        return this._visible;
    },

    set visible(flag)
    {
        this._visible = flag;
    },

    get scrollableElements()
    {
        // Implemented by subclasses.
        return [];
    },

    get shouldKeepElementsScrolledToBottom()
    {
        // Implemented by subclasses.
        return false;
    },

    get selectionPathComponents()
    {
        // Implemented by subclasses.
        return [];
    },

    get supplementalRepresentedObjects()
    {
        // Implemented by subclasses.
        return [];
    },

    get supportsSplitContentBrowser()
    {
        // Implemented by subclasses.
        return true;
    },

    updateLayout: function()
    {
        // Implemented by subclasses.
    },

    shown: function()
    {
        // Implemented by subclasses.
    },

    hidden: function()
    {
        // Implemented by subclasses.
    },

    closed: function()
    {
        // Implemented by subclasses.
    },

    saveToCookie: function(cookie)
    {
        // Implemented by subclasses.
    },

    restoreFromCookie: function(cookie)
    {
        // Implemented by subclasses.
    },

    canGoBack: function()
    {
        // Implemented by subclasses.
        return false;
    },

    canGoForward: function()
    {
        // Implemented by subclasses.
        return false;
    },

    goBack: function()
    {
        // Implemented by subclasses.
    },

    goForward: function()
    {
        // Implemented by subclasses.
    },

    get supportsSearch()
    {
        // Implemented by subclasses.
        return false;
    },

    get numberOfSearchResults()
    {
        // Implemented by subclasses.
        return null;
    },

    get hasPerformedSearch()
    {
        // Implemented by subclasses.
        return false;
    },

    set automaticallyRevealFirstSearchResult(reveal)
    {
        // Implemented by subclasses.
    },

    performSearch: function(query)
    {
        // Implemented by subclasses.
    },

    searchCleared: function()
    {
        // Implemented by subclasses.
    },

    searchQueryWithSelection: function()
    {
        // Implemented by subclasses.
        return null;
    },

    revealPreviousSearchResult: function(changeFocus)
    {
        // Implemented by subclasses.
    },

    revealNextSearchResult: function(changeFocus)
    {
        // Implemented by subclasses.
    }
};

WebInspector.ContentView.prototype.__proto__ = WebInspector.Object.prototype;
