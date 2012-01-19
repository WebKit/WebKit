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
 * @implements {WebInspector.ScriptsPanel.EditorContainer}
 * @constructor
 */
WebInspector.TabbedEditorContainer = function()
{
    this._tabbedPane = new WebInspector.TabbedPane();
    this._tabbedPane.closeableTabs = true;
    this._tabbedPane.element.id = "scripts-editor-container-tabbed-pane";

    this._tabbedPane.addEventListener(WebInspector.TabbedPane.EventTypes.TabClosed, this._tabClosed, this);
    this._tabbedPane.addEventListener(WebInspector.TabbedPane.EventTypes.TabSelected, this._tabSelected, this);

    this._titles = new Map();
    this._tooltips = new Map();
    this._tabIds = new Map();  
}

WebInspector.TabbedEditorContainer._tabId = 0;

WebInspector.TabbedEditorContainer.prototype = {
    /**
     * @type {WebInspector.SourceFrame}
     */
    get currentSourceFrame()
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
     * @param {string} title
     * @param {WebInspector.SourceFrame} sourceFrame
     * @param {string} tooltip
     */
    showSourceFrame: function(title, sourceFrame, tooltip)
    {
        var tabId = this._tabIds.get(sourceFrame) || this._appendSourceFrameTab(title, sourceFrame, tooltip);
        this._tabbedPane.selectTab(tabId);
    },

    /**
     * @param {string} title
     * @param {WebInspector.SourceFrame} sourceFrame
     * @param {string} tooltip
     */
    _appendSourceFrameTab: function(title, sourceFrame, tooltip)
    {
        var tabId = this._generateTabId();
        this._tabIds.put(sourceFrame, tabId)
        this._titles.put(sourceFrame, title)
        this._tooltips.put(sourceFrame, tooltip)
        
        this._tabbedPane.appendTab(tabId, title, sourceFrame, tooltip);
        return tabId;
    },

    /**
     * @param {WebInspector.SourceFrame} sourceFrame
     */
    _removeSourceFrameTab: function(sourceFrame)
    {
        var tabId = this._tabIds.get(sourceFrame);
        
        if (tabId)
            this._tabbedPane.closeTab(tabId);
    },

    /**
     * @param {WebInspector.Event} event
     */
    _tabClosed: function(event)
    {
        var sourceFrame = /** @type {WebInspector.UISourceCode} */ event.data.view;
        this._tabIds.remove(sourceFrame);
        this._titles.remove(sourceFrame);
        this._tooltips.remove(sourceFrame);
    },

    /**
     * @param {WebInspector.Event} event
     */
    _tabSelected: function(event)
    {
        var sourceFrame = /** @type {WebInspector.UISourceCode} */ event.data.view;
        this.dispatchEventToListeners(WebInspector.ScriptsPanel.EditorContainer.Events.EditorSelected, sourceFrame);
    },

    /**
     * @param {WebInspector.SourceFrame} oldSourceFrame
     * @param {string} title
     * @param {WebInspector.SourceFrame} sourceFrame
     * @param {string} tooltip
     */
    _replaceSourceFrameTab: function(oldSourceFrame, title, sourceFrame, tooltip)
    {
        var tabId = this._tabIds.get(oldSourceFrame);
        
        if (tabId) {
            this._tabIds.remove(oldSourceFrame);
            this._titles.remove(oldSourceFrame);
            this._tooltips.remove(oldSourceFrame);

            this._tabIds.put(sourceFrame, tabId);
            this._titles.put(sourceFrame, title);
            this._tooltips.put(sourceFrame, tooltip);

            this._tabbedPane.changeTabTitle(tabId, title);
            this._tabbedPane.changeTabView(tabId, sourceFrame);
            this._tabbedPane.changeTabTooltip(tabId, tooltip);
        }
    },

    /**
     * @param {WebInspector.SourceFrame} sourceFrame
     * @return {boolean}
     */
    isSourceFrameOpen: function(sourceFrame)
    {
        return !!this._tabIds.get(sourceFrame);
    },

    /**
     * @param {Array.<WebInspector.SourceFrame>} oldSourceFrames
     * @param {string} title
     * @param {WebInspector.SourceFrame} sourceFrame
     * @param {string} tooltip
     */
    replaceSourceFrames: function(oldSourceFrames, title, sourceFrame, tooltip)
    {
        var mainSourceFrame;
        for (var i = 0; i < oldSourceFrames.length; ++i) {
            var tabId = this._tabIds.get(oldSourceFrames[i]);
            if (tabId && (!mainSourceFrame || this._tabbedPane.selectedTabId === tabId)) {
                mainSourceFrame = oldSourceFrames[i];
                break;
            } 
        }
        
        if (mainSourceFrame)
            this._replaceSourceFrameTab(mainSourceFrame, title, sourceFrame, tooltip);
        
        for (var i = 0; i < oldSourceFrames.length; ++i)
            this._removeSourceFrameTab(oldSourceFrames[i]);
    },

    /**
     * @param {WebInspector.SourceFrame} sourceFrame
     * @param {boolean} isDirty
     */
    setSourceFrameIsDirty: function(sourceFrame, isDirty)
    {
        var tabId = this._tabIds.get(sourceFrame);
        if (tabId) {
            var title = this._titles.get(sourceFrame);
            if (isDirty)
                title += "*";
            this._tabbedPane.changeTabTitle(tabId, title);
        }
    },

    reset: function()
    {
        this._tabbedPane.closeAllTabs();
        this._titles = new Map();
        this._tooltips = new Map();
        this._tabIds = new Map();  
    },

    /**
     * @return {string}
     */
    _generateTabId: function()
    {
        return "tab_" + (WebInspector.TabbedEditorContainer._tabId++);
    }
}

WebInspector.TabbedEditorContainer.prototype.__proto__ = WebInspector.Object.prototype;