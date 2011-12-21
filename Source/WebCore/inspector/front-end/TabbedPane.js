/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/**
 * @extends {WebInspector.View}
 * @constructor
 */
WebInspector.TabbedPane = function()
{
    WebInspector.View.call(this);
    this.element.addStyleClass("tabbed-pane");
    this._headerElement = this.element.createChild("div", "tabbed-pane-header");
    this._tabsElement = this._headerElement.createChild("div", "tabbed-pane-header-tabs");
    this._contentElement = this.element.createChild("div", "tabbed-pane-content");
    this._tabs = {};
    
    this._tabsThatNeedMeasuring = [];
}

WebInspector.TabbedPane.prototype = {
    /**
     * @param {string} id
     * @param {string} tabTitle
     * @param {WebInspector.View} view
     */
    appendTab: function(id, tabTitle, view)
    {
        var tabElement = this._createTabElement(id, tabTitle, false);
        tabElement.addEventListener("click", this.selectTab.bind(this, id, true), false);
        this._tabsElement.appendChild(tabElement);

        var tab = new WebInspector.TabbedPaneTab(id, tabTitle, tabElement, view);
        this._tabs[id] = tab;
        
        this._tabsThatNeedMeasuring.push(tab);
        this._maybeMeasureAndUpdate();
    },

    /**
     * @param {string} id
     * @param {string} title
     * @param {boolean} measuring
     */
    _createTabElement: function(id, title, measuring)
    {
        var tabElement = document.createElement("div");
        tabElement.addStyleClass("tabbed-pane-header-tab");
        if (measuring)
            tabElement.addStyleClass("measuring");

        tabElement.textContent = title;

        return tabElement;
    },

    onResize: function()
    {
        this._maybeMeasureAndUpdate();
    },

    _maybeMeasureAndUpdate: function()
    {
        if (!this.isShowing())
            return;
        
        for (var i = 0; i < this._tabsThatNeedMeasuring.length; ++i)
            this._measureTab(this._tabsThatNeedMeasuring[i]);
        this._tabsThatNeedMeasuring = [];
        
        this._updateWidths();
    },

    /**
     * @param {WebInspector.TabbedPaneTab} tab
     */
    _measureTab: function(tab)
    {
        var measuredTabElement = this._createTabElement(tab.id, tab.title, true);

        this._tabsElement.appendChild(measuredTabElement);
        tab.measuredWidth = measuredTabElement.offsetWidth;
        this._tabsElement.removeChild(measuredTabElement);
    },
    
    _updateWidths: function()
    {
        var measuredWidths = [];
        for (var tabId in this._tabs)
            measuredWidths.push(this._tabs[tabId].measuredWidth);
        
        var maxWidth = this._calculateMaxWidth(measuredWidths, this._tabsElement.offsetWidth);
        for (var tabId in this._tabs) {
            var tab = this._tabs[tabId];
            var width = Math.min(tab.measuredWidth, maxWidth);
            tab.tabElement.style.width = width + "px";
        }
    },
    
    /**
     * @param {Array.<number>} measuredWidths
     * @param {number} totalWidth
     */
    _calculateMaxWidth: function(measuredWidths, totalWidth)
    {
        if (!measuredWidths.length)
            return 0;
        
        measuredWidths.sort();
        
        var totalMeasuredWidth = 0;
        for (var i = 0; i < measuredWidths.length; ++i)
            totalMeasuredWidth += measuredWidths[i];
        
        if (totalWidth >= totalMeasuredWidth)
            return measuredWidths[measuredWidths.length - 1];
        
        var totalExtraWidth = 0;
        for (var i = measuredWidths.length - 1; i > 0; --i) {
            var extraWidth = measuredWidths[i] - measuredWidths[i - 1];
            totalExtraWidth += (measuredWidths.length - i) * extraWidth;

            if (totalWidth + totalExtraWidth >= totalMeasuredWidth)
                return measuredWidths[i - 1] + (totalWidth + totalExtraWidth - totalMeasuredWidth) / (measuredWidths.length - i); 
        }
        
        return totalWidth / measuredWidths.length;
    },    

    /**
     * @param {string} id
     * @param {boolean=} userGesture
     */
    selectTab: function(id, userGesture)
    {
        if (!(id in this._tabs))
            return false;

        if (this._currentTab) {
            if (this._currentTab.id === id)
                return;
            this._hideTab(this._currentTab)
            delete this._currentTab;
        }

        var tab = this._tabs[id];
        this._showTab(tab);
        this._currentTab = tab;
        var event = {tabId: id, view: tab.view, isUserGesture: userGesture};
        this.dispatchEventToListeners("tab-selected", event);
        return true;
    },

    /**
     * @param {WebInspector.TabbedPaneTab} tab
     */
    _showTab: function(tab)
    {
        tab.tabElement.addStyleClass("selected");
        tab.view.show(this._contentElement);
    },

    /**
     * @param {WebInspector.TabbedPaneTab} tab
     */
    _hideTab: function(tab)
    {
        tab.tabElement.removeStyleClass("selected");
        tab.view.detach();
    },

    canHighlightLine: function()
    {
        return this._currentTab && this._currentTab.view && this._currentTab.view.canHighlightLine();
    },

    highlightLine: function(line)
    {
        if (this.canHighlightLine())
            this._currentTab.view.highlightLine(line);
    },

    /**
     * @return {Array.<Element>}
     */
    elementsToRestoreScrollPositionsFor: function()
    {
        return [ this._contentElement ];
    }
}

WebInspector.TabbedPane.prototype.__proto__ = WebInspector.View.prototype;


/**
 * @constructor
 * @param {string} id
 * @param {string} title
 * @param {Element} tabElement
 * @param {WebInspector.View} view
 */
WebInspector.TabbedPaneTab = function(id, title, tabElement, view)
{
    this.title = title;
    this.id = id;
    this.tabElement = tabElement;
    this.view = view;
}
