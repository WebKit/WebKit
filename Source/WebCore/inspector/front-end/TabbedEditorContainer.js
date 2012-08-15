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
 * @interface
 */
WebInspector.TabbedEditorContainerDelegate = function() { }

WebInspector.TabbedEditorContainerDelegate.prototype = {
    /**
     * @param {WebInspector.UISourceCode} uiSourceCode
     * @return {WebInspector.SourceFrame}
     */
    viewForFile: function(uiSourceCode) { }
}

/**
 * @constructor
 * @extends {WebInspector.Object}
 * @param {WebInspector.TabbedEditorContainerDelegate} delegate
 * @param {string} settingName
 */
WebInspector.TabbedEditorContainer = function(delegate, settingName)
{
    WebInspector.Object.call(this);
    this._delegate = delegate;

    this._tabbedPane = new WebInspector.TabbedPane();
    this._tabbedPane.closeableTabs = true;
    this._tabbedPane.element.id = "scripts-editor-container-tabbed-pane";

    this._tabbedPane.addEventListener(WebInspector.TabbedPane.EventTypes.TabClosed, this._tabClosed, this);
    this._tabbedPane.addEventListener(WebInspector.TabbedPane.EventTypes.TabSelected, this._tabSelected, this);

    this._tabIds = new Map();
    this._files = {};
    this._loadedURLs = {};

    this._previouslyViewedFilesSetting = WebInspector.settings.createSetting(settingName, []);
    this._history = WebInspector.TabbedEditorContainer.History.fromObject(this._previouslyViewedFilesSetting.get());
}


WebInspector.TabbedEditorContainer.Events = {
    EditorSelected: "EditorSelected",
    EditorClosed: "EditorClosed"
}

WebInspector.TabbedEditorContainer._tabId = 0;

WebInspector.TabbedEditorContainer.maximalPreviouslyViewedFilesCount = 30;

WebInspector.TabbedEditorContainer.prototype = {
    /**
     * @return {WebInspector.View}
     */
    get view()
    {
        return this._tabbedPane;
    },

    /**
     * @type {WebInspector.SourceFrame}
     */
    get visibleView()
    {
        return this._tabbedPane.visibleView;
    },

    /**
     * @param {Element} parentElement
     */
    show: function(parentElement)
    {
        this._tabbedPane.show(parentElement);
    },

    /**
     * @param {WebInspector.UISourceCode} uiSourceCode
     */
    showFile: function(uiSourceCode)
    {
        this._innerShowFile(uiSourceCode, true);
    },

    _addScrollAndSelectionListeners: function()
    {
        console.assert(this._currentFile);
        var sourceFrame = this._delegate.viewForFile(this._currentFile);
        sourceFrame.addEventListener(WebInspector.SourceFrame.Events.ScrollChanged, this._scrollChanged, this);
        sourceFrame.addEventListener(WebInspector.SourceFrame.Events.SelectionChanged, this._selectionChanged, this);
    },

    _removeScrollAndSelectionListeners: function()
    {
        if (!this._currentFile)
            return;
        var sourceFrame = this._delegate.viewForFile(this._currentFile);
        sourceFrame.removeEventListener(WebInspector.SourceFrame.Events.ScrollChanged, this._scrollChanged, this);
        sourceFrame.removeEventListener(WebInspector.SourceFrame.Events.SelectionChanged, this._selectionChanged, this);
    },

    _scrollChanged: function(event)
    {
        var lineNumber = /** @type {number} */ event.data;
        this._history.updateScrollLineNumber(this._currentFile.url, lineNumber);
        this._history.save(this._previouslyViewedFilesSetting);
    },

    _selectionChanged: function(event)
    {
        var range = /** @type {WebInspector.TextRange} */ event.data;
        this._history.updateSelectionRange(this._currentFile.url, range);
        this._history.save(this._previouslyViewedFilesSetting);
    },

    /**
     * @param {WebInspector.UISourceCode} uiSourceCode
     * @param {boolean=} userGesture
     */
    _innerShowFile: function(uiSourceCode, userGesture)
    {
        if (this._currentFile === uiSourceCode)
            return;
        this._removeScrollAndSelectionListeners();
        this._currentFile = uiSourceCode;

        var tabId = this._tabIds.get(uiSourceCode) || this._appendFileTab(uiSourceCode, userGesture);
        
        this._tabbedPane.selectTab(tabId, userGesture);
        if (userGesture)
            this._editorSelectedByUserAction();
        
        this._addScrollAndSelectionListeners();
        
        this.dispatchEventToListeners(WebInspector.TabbedEditorContainer.Events.EditorSelected, this._currentFile);
    },

    /**
     * @param {WebInspector.UISourceCode} uiSourceCode
     * @return {string}
     */
    _titleForFile: function(uiSourceCode)
    {
        const maxDisplayNameLength = 30;
        const minDisplayQueryParamLength = 5;

        var title;
        var parsedURL = uiSourceCode.parsedURL;
        if (!parsedURL.isValid)
            title = parsedURL.url ? parsedURL.url.trimMiddle(maxDisplayNameLength) : WebInspector.UIString("(program)");
        else {
            var maxDisplayQueryParamLength = Math.max(minDisplayQueryParamLength, maxDisplayNameLength - parsedURL.lastPathComponent.length);
            var displayQueryParams = parsedURL.queryParams ? "?" + parsedURL.queryParams.trimEnd(maxDisplayQueryParamLength - 1) : "";
            var displayLastPathComponent = parsedURL.lastPathComponent.trimMiddle(maxDisplayNameLength - displayQueryParams.length);
            var displayName = displayLastPathComponent + displayQueryParams;
            title = displayName || WebInspector.UIString("(program)");
        }
        
        if (uiSourceCode.isDirty())
            title += "*";
        return title;
    },

    /**
     * @param {WebInspector.UISourceCode} uiSourceCode
     */
    addUISourceCode: function(uiSourceCode)
    {
        if (this._userSelectedFiles || this._loadedURLs[uiSourceCode.url])
            return;
        this._loadedURLs[uiSourceCode.url] = true;

        var index = this._history.index(uiSourceCode.url)
        if (index === -1)
            return;

        var tabId = this._tabIds.get(uiSourceCode) || this._appendFileTab(uiSourceCode, false);

        // Select tab if this file was the last to be shown.
        if (index === 0)
            this._innerShowFile(uiSourceCode, false);
    },

    /**
     * @param {WebInspector.UISourceCode} uiSourceCode
     */
    removeUISourceCode: function(uiSourceCode)
    {
        var tabId = this._tabIds.get(uiSourceCode);
        if (tabId)
            this._tabbedPane.closeTab(tabId);
    },

    /**
     * @param {WebInspector.UISourceCode} uiSourceCode
     */
    _editorClosedByUserAction: function(uiSourceCode)
    {
        this._userSelectedFiles = true;
        this._history.remove(uiSourceCode.url);
        this._updateHistory();
    },

    _editorSelectedByUserAction: function()
    {
        this._userSelectedFiles = true;
        this._updateHistory();
    },

    _updateHistory: function()
    {
        var tabIds = this._tabbedPane.lastOpenedTabIds(WebInspector.TabbedEditorContainer.maximalPreviouslyViewedFilesCount);
        
        function tabIdToURL(tabId)
        {
            return this._files[tabId].url;
        }
        
        this._history.update(tabIds.map(tabIdToURL.bind(this)));
        this._history.save(this._previouslyViewedFilesSetting);
    },

    /**
     * @param {WebInspector.UISourceCode} uiSourceCode
     * @return {string}
     */
    _tooltipForFile: function(uiSourceCode)
    {
        return uiSourceCode.url;
    },

    /**
     * @param {WebInspector.UISourceCode} uiSourceCode
     * @param {boolean=} userGesture
     */
    _appendFileTab: function(uiSourceCode, userGesture)
    {
        var view = this._delegate.viewForFile(uiSourceCode);
        var title = this._titleForFile(uiSourceCode);
        var tooltip = this._tooltipForFile(uiSourceCode);

        var tabId = this._generateTabId();
        this._tabIds.put(uiSourceCode, tabId);
        this._files[tabId] = uiSourceCode;

        var savedScrollLineNumber = this._history.scrollLineNumber(uiSourceCode.url);
        if (savedScrollLineNumber)
            view.scrollToLine(savedScrollLineNumber);
        var savedSelectionRange = this._history.selectionRange(uiSourceCode.url);
        if (savedSelectionRange)
            view.setSelection(savedSelectionRange);

        this._tabbedPane.appendTab(tabId, title, view, tooltip, userGesture);

        this._addUISourceCodeListeners(uiSourceCode);
        return tabId;
    },

    /**
     * @param {WebInspector.Event} event
     */
    _tabClosed: function(event)
    {
        var tabId = /** @type {string} */ event.data.tabId;
        var userGesture = /** @type {boolean} */ event.data.isUserGesture;

        var uiSourceCode = this._files[tabId];
        if (this._currentFile === uiSourceCode) {
            this._removeScrollAndSelectionListeners();
            delete this._currentFile;
        }
        this._tabIds.remove(uiSourceCode);
        delete this._files[tabId];

        this._removeUISourceCodeListeners(uiSourceCode);

        this.dispatchEventToListeners(WebInspector.TabbedEditorContainer.Events.EditorClosed, uiSourceCode);

        if (userGesture)
            this._editorClosedByUserAction(uiSourceCode);
    },

    /**
     * @param {WebInspector.Event} event
     */
    _tabSelected: function(event)
    {
        var tabId = /** @type {string} */ event.data.tabId;
        var userGesture = /** @type {boolean} */ event.data.isUserGesture;

        var uiSourceCode = this._files[tabId];
        this._innerShowFile(uiSourceCode, userGesture);
    },

    /**
     * @param {WebInspector.UISourceCode} oldUISourceCode
     * @param {WebInspector.UISourceCode} uiSourceCode
     */
    replaceFile: function(oldUISourceCode, uiSourceCode)
    {
        var tabId = this._tabIds.get(oldUISourceCode);
        
        if (!tabId)
            return;
        
        delete this._files[this._tabIds.get(oldUISourceCode)]
        this._tabIds.remove(oldUISourceCode);
        this._tabIds.put(uiSourceCode, tabId);
        this._files[tabId] = uiSourceCode;

        this._tabbedPane.changeTabTitle(tabId, this._titleForFile(uiSourceCode));
        this._tabbedPane.changeTabView(tabId, this._delegate.viewForFile(uiSourceCode));
        this._tabbedPane.changeTabTooltip(tabId, this._tooltipForFile(uiSourceCode));

        this._removeUISourceCodeListeners(oldUISourceCode);
        this._addUISourceCodeListeners(uiSourceCode);
    },

    /**
     * @param {WebInspector.UISourceCode} uiSourceCode
     */
    _addUISourceCodeListeners: function(uiSourceCode)
    {
        uiSourceCode.addEventListener(WebInspector.UISourceCode.Events.TitleChanged, this._uiSourceCodeTitleChanged, this);
        uiSourceCode.addEventListener(WebInspector.UISourceCode.Events.WorkingCopyChanged, this._uiSourceCodeWorkingCopyChanged, this);
        uiSourceCode.addEventListener(WebInspector.UISourceCode.Events.ContentChanged, this._uiSourceCodeContentChanged, this);
    },

    /**
     * @param {WebInspector.UISourceCode} uiSourceCode
     */
    _removeUISourceCodeListeners: function(uiSourceCode)
    {
        uiSourceCode.removeEventListener(WebInspector.UISourceCode.Events.TitleChanged, this._uiSourceCodeTitleChanged, this);
        uiSourceCode.removeEventListener(WebInspector.UISourceCode.Events.WorkingCopyChanged, this._uiSourceCodeWorkingCopyChanged, this);
        uiSourceCode.removeEventListener(WebInspector.UISourceCode.Events.ContentChanged, this._uiSourceCodeContentChanged, this);
    },

    /**
     * @param {WebInspector.UISourceCode} uiSourceCode
     */
    _updateFileTitle: function(uiSourceCode)
    {
        var tabId = this._tabIds.get(uiSourceCode);
        if (tabId) {
            var title = this._titleForFile(uiSourceCode);
            this._tabbedPane.changeTabTitle(tabId, title);
        }
    },

    _uiSourceCodeTitleChanged: function(event)
    {
        var uiSourceCode = /** @type {WebInspector.UISourceCode} */ event.target;
        this._updateFileTitle(uiSourceCode);
    },

    _uiSourceCodeWorkingCopyChanged: function(event)
    {
        var uiSourceCode = /** @type {WebInspector.UISourceCode} */ event.target;
        this._updateFileTitle(uiSourceCode);
    },

    _uiSourceCodeContentChanged: function(event)
    {
        var uiSourceCode = /** @type {WebInspector.UISourceCode} */ event.target;
        this._updateFileTitle(uiSourceCode);
    },

    reset: function()
    {
        this._tabbedPane.closeAllTabs();
        this._tabIds = new Map();
        this._files = {};
        delete this._currentFile;
        delete this._userSelectedFiles;
        this._loadedURLs = {};
    },

    /**
     * @return {string}
     */
    _generateTabId: function()
    {
        return "tab_" + (WebInspector.TabbedEditorContainer._tabId++);
    },

    /**
     * @return {WebInspector.UISourceCode} uiSourceCode
     */
    currentFile: function()
    {
        return this._currentFile;
    }
}

WebInspector.TabbedEditorContainer.prototype.__proto__ = WebInspector.Object.prototype;

/**
 * @constructor
 * @param {string} url
 * @param {WebInspector.TextRange=} selectionRange
 * @param {number=} scrollLineNumber
 */
WebInspector.TabbedEditorContainer.HistoryItem = function(url, selectionRange, scrollLineNumber)
{
    this.url = url;
    this.selectionRange = selectionRange;
    this.scrollLineNumber = scrollLineNumber;
}

/**
 * @param {Object} serializedHistoryItem
 * @return {WebInspector.TabbedEditorContainer.HistoryItem}
 */
WebInspector.TabbedEditorContainer.HistoryItem.fromObject = function (serializedHistoryItem)
{
    var selectionRange = serializedHistoryItem.selectionRange ? WebInspector.TextRange.fromObject(serializedHistoryItem.selectionRange) : null;
    return new WebInspector.TabbedEditorContainer.HistoryItem(serializedHistoryItem.url, selectionRange, serializedHistoryItem.scrollLineNumber);
}

WebInspector.TabbedEditorContainer.HistoryItem.prototype = {
    /**
     * @return {Object}
     */
    serializeToObject: function()
    {
        var serializedHistoryItem = {};
        serializedHistoryItem.url = this.url;
        serializedHistoryItem.selectionRange = this.selectionRange;
        serializedHistoryItem.scrollLineNumber = this.scrollLineNumber;
        return serializedHistoryItem;
    }
}

WebInspector.TabbedEditorContainer.HistoryItem.prototype.__proto__ = WebInspector.Object.prototype;

/**
 * @constructor
 * @param {Array.<WebInspector.TabbedEditorContainer.HistoryItem>} items
 */
WebInspector.TabbedEditorContainer.History = function(items)
{
    this._items = items;
}

/**
 * @param {Object} serializedHistory
 * @return {WebInspector.TabbedEditorContainer.History}
 */
WebInspector.TabbedEditorContainer.History.fromObject = function(serializedHistory)
{
    var items = [];
    for (var i = 0; i < serializedHistory.length; ++i)
        items.push(WebInspector.TabbedEditorContainer.HistoryItem.fromObject(serializedHistory[i]));
    return new WebInspector.TabbedEditorContainer.History(items);
}

WebInspector.TabbedEditorContainer.History.prototype = {
    /**
     * @param {string} url
     * @return {number}
     */
    index: function(url)
    {
        for (var i = 0; i < this._items.length; ++i) {
            if (this._items[i].url === url)
                return i;
        }
        return -1;
    },

    /**
     * @param {string} url
     * @return {WebInspector.TextRange|undefined}
     */
    selectionRange: function(url)
    {
        var index = this.index(url);
        return index !== -1 ? this._items[index].selectionRange : undefined;
    },

    /**
     * @param {string} url
     * @param {WebInspector.TextRange} selectionRange
     */
    updateSelectionRange: function(url, selectionRange)
    {
        if (!selectionRange)
            return;
        var index = this.index(url);
        if (index === -1)
            return;
        this._items[index].selectionRange = selectionRange;
    },

    /**
     * @param {string} url
     * @return {number|undefined}
     */
    scrollLineNumber: function(url)
    {
        var index = this.index(url);
        return index !== -1 ? this._items[index].scrollLineNumber : undefined;
    },

    /**
     * @param {string} url
     * @param {number} scrollLineNumber
     */
    updateScrollLineNumber: function(url, scrollLineNumber)
    {
        var index = this.index(url);
        if (index === -1)
            return;
        this._items[index].scrollLineNumber = scrollLineNumber;
    },

    /**
     * @param {Array.<string>} urls
     */
    update: function(urls)
    {
        for (var i = urls.length - 1; i >= 0; --i) {
            var index = this.index(urls[i]);
            var item;
            if (index !== -1) {
                item = this._items[index];
                this._items.splice(index, 1);
            } else
                item = new WebInspector.TabbedEditorContainer.HistoryItem(urls[i]);
            this._items.unshift(item);
        }
    },

    /**
     * @param {string} url
     */
    remove: function(url)
    {
        var index = this.index(url);
        if (index !== -1)
            this._items.splice(index, 1);
    },
    
    /**
     * @param {WebInspector.Setting} setting
     */
    save: function(setting)
    {
        setting.set(this._serializeToObject());
    },
    
    /**
     * @return {Object}
     */
    _serializeToObject: function()
    {
        var serializedHistory = [];
        for (var i = 0; i < this._items.length; ++i)
            serializedHistory.push(this._items[i].serializeToObject());
        return serializedHistory;
    }
}

WebInspector.TabbedEditorContainer.History.prototype.__proto__ = WebInspector.Object.prototype;
