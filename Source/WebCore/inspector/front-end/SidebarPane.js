/*
 * Copyright (C) 2007 Apple Inc.  All rights reserved.
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
 * @extends {WebInspector.View}
 */
WebInspector.SidebarPane = function(title)
{
    WebInspector.View.call(this);
    this.element.className = "sidebar-pane";

    this.titleElement = document.createElement("div");
    this.titleElement.className = "sidebar-pane-toolbar";

    this.bodyElement = this.element.createChild("div", "body");

    this._title = title;

    this._expandCallback = null;
    this._showCallback = null;
}

WebInspector.SidebarPane.prototype = {
    title: function()
    {
        return this._title;
    },

    /**
     * @param {function()} callback
     */
    prepareContent: function(callback)
    {
        if (callback)
            callback();
    },

    expanded: function()
    {
        return this._expanded;
    },

    expand: function()
    {
        this._expanded = true;
        this.prepareContent(this.onContentReady.bind(this));
    },

    collapse: function()
    {
        this._expanded = false;
    },

    onContentReady: function()
    {
        if (this._expandCallback)
            this._expandCallback();
    },

    /**
     * @param {function()} callback
     */
    _setExpandCallback: function(callback)
    {
        this._expandCallback = callback;
    },

    /**
     * @param {function()} callback
     */
    setShowCallback: function(callback)
    {
        this._showCallback = callback;
    },

    wasShown: function()
    {
        WebInspector.View.prototype.wasShown.call(this);
        if (this._showCallback)
            this._showCallback();
    },

    __proto__: WebInspector.View.prototype
}

/**
 * @constructor
 * @extends {WebInspector.View}
 */
WebInspector.SidebarPaneStack = function()
{
    WebInspector.View.call(this);
    this.element.className = "sidebar-pane-stack fill";

    this._titles = [];
    this._panes = [];
}

WebInspector.SidebarPaneStack.prototype = {
    /**
     * @param {WebInspector.SidebarPane} pane
     */
    addPane: function(pane)
    {
        var index = this._panes.length;
        this._panes.push(pane);

        var title = this.element.createChild("div", "pane-title");
        title.textContent = pane.title();
        title.tabIndex = 0;
        title.addEventListener("click", this._togglePane.bind(this, index), false);
        title.addEventListener("keydown", this._onTitleKeyDown.bind(this, index), false);
        this._titles.push(title);

        pane.titleElement.removeSelf();
        title.appendChild(pane.titleElement);

        pane._setExpandCallback(this._onPaneExpanded.bind(this, index));
        this._setExpanded(index, pane.expanded());
    },

    activePaneId: function()
    {
        return this._activePaneIndex;
    },

    /**
     * @param {number} index
     */
    setActivePaneId: function(index)
    {
        this._panes[index].expand();
    },

    /**
     * @param {number} index
     */
    _isExpanded: function(index)
    {
        var title = this._titles[index];
        return title.hasStyleClass("expanded");
    },

    /**
     * @param {number} index
     * @param {boolean} on
     */
    _setExpanded: function(index, on)
    {
        if (on)
            this._panes[index].expand();
        else
            this._collapsePane(index);
    },

    /**
     * @param {number} index
     */
    _onPaneExpanded: function(index)
    {
        this._activePaneIndex = index;
        var pane = this._panes[index];
        var title = this._titles[index];
        title.addStyleClass("expanded");
        pane.show(this.element, title.nextSibling);
    },

    /**
     * @param {number} index
     */
    _collapsePane: function(index)
    {
        var pane = this._panes[index];
        var title = this._titles[index];
        title.removeStyleClass("expanded");
        pane.collapse();
        if (pane.element.parentNode == this.element)
            pane.detach();
    },

    /**
     * @param {number} index
     * @private
     */
    _togglePane: function(index)
    {
        this._setExpanded(index, !this._isExpanded(index));
    },
    
    /**
     * @param {number} index
     * @param {Event} event
     * @private
     */
    _onTitleKeyDown: function(index, event)
    {
        if (isEnterKey(event) || event.keyCode === WebInspector.KeyboardShortcut.Keys.Space.code)
            this._togglePane(index);
    },

    __proto__: WebInspector.View.prototype
}

/**
 * @constructor
 * @extends {WebInspector.TabbedPane}
 */
WebInspector.SidebarTabbedPane = function()
{
    WebInspector.TabbedPane.call(this);
    this.element.addStyleClass("sidebar-tabbed-pane");

    this._panes = [];
}

WebInspector.SidebarTabbedPane.prototype = {
    /**
     * @param {WebInspector.SidebarPane} pane
     */
    addPane: function(pane)
    {
        var index = this._panes.length;
        this._panes.push(pane);
        this.appendTab(index.toString(), pane.title(), pane);

        pane.titleElement.removeSelf();
        pane.element.appendChild(pane.titleElement);

        pane._setExpandCallback(this.setActivePaneId.bind(this, index));
    },

    activePaneId: function()
    {
        return this.selectedTabId;
    },

    /**
     * @param {number} index
     */
    setActivePaneId: function(index)
    {
        this.selectTab(index.toString());
    },

    __proto__: WebInspector.TabbedPane.prototype
}

/**
 * @constructor
 * @extends {WebInspector.View}
 */
WebInspector.SidebarPaneGroup = function()
{
    WebInspector.View.call(this);
    this.element.className = "fill";

    this._panes = [];
}

WebInspector.SidebarPaneGroup.prototype = {
    /**
     * @param {boolean} stacked
     */
    setStacked: function(stacked)
    {
        if (this._stacked === stacked)
            return;

        this._stacked = stacked;

        var activePaneId;
        if (this._currentView) {
            activePaneId = this._currentView.activePaneId();
            this._currentView.detach();
        }

        if (this._stacked)
            this._currentView = new WebInspector.SidebarPaneStack();
        else
            this._currentView = new WebInspector.SidebarTabbedPane();

        for (var i = 0; i < this._panes.length; i++)
            this._currentView.addPane(this._panes[i]);

        this._currentView.show(this.element);

        if (typeof activePaneId !== "undefined")
            this._currentView.setActivePaneId(activePaneId);
    },

    /**
     * @param {WebInspector.SidebarPane} pane
     */
    addPane: function(pane)
    {
        this._panes.push(pane);
        if (this._currentView)
            this._currentView.addPane(pane);
    },

    /**
     * @param {WebInspector.Panel} panel
     */
    attachToPanel: function(panel)
    {
        this._sidebarView = panel.splitView;

        this._sidebarView.sidebarElement.addEventListener("contextmenu", this._contextMenuEventFired.bind(this), false);

        var splitDirectionSettingName = panel.name + "PanelSplitHorizontally";
        if (!WebInspector.settings[splitDirectionSettingName])
            WebInspector.settings[splitDirectionSettingName] = WebInspector.settings.createSetting(splitDirectionSettingName, false);
        this._splitDirectionSetting = WebInspector.settings[splitDirectionSettingName];
        this._splitDirectionSetting.addChangeListener(this._onSplitDirectionSettingChanged.bind(this));

        this._updateSplitDirection();

        this.show(this._sidebarView.sidebarElement);
    },

    populateContextMenu: function(contextMenu)
    {
        if (!WebInspector.experimentsSettings.horizontalPanelSplit.isEnabled())
            return;

        function toggleSplitDirection()
        {
            this._splitDirectionSetting.set(!this._splitDirectionSetting.get());
        }
        contextMenu.appendCheckboxItem(WebInspector.UIString(WebInspector.useLowerCaseMenuTitles() ? "Split horizontally" : "Split Horizontally"), toggleSplitDirection.bind(this), this._splitDirectionSetting.get());
    },

    _contextMenuEventFired: function(event)
    {
        var contextMenu = new WebInspector.ContextMenu(event);
        this.populateContextMenu(contextMenu);
        contextMenu.show();
    },

    _updateSplitDirection: function()
    {
        var vertical = !WebInspector.experimentsSettings.horizontalPanelSplit.isEnabled() || !this._splitDirectionSetting.get();
        this._sidebarView.setVertical(vertical);
        this.setStacked(vertical);
    },

    _onSplitDirectionSettingChanged: function()
    {
        // Cannot call _updateSplitDirection directly because View.prototype.show() does not work properly from inside notifications.
        setTimeout(this._updateSplitDirection.bind(this), 0);
    },

    __proto__: WebInspector.View.prototype
}

