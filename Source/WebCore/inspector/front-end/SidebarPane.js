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
    this.element.className = "pane";

    this.titleElement = document.createDocumentFragment();
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
     * @param {function} callback
     */
    prepareContent: function(callback)
    {
        callback();
    },

    expand: function()
    {
        if (this._expandCallback)
            this.prepareContent(this.onContentReady.bind(this));
        else
            this._expandPending = true;
    },

    onContentReady: function()
    {
        this._expandCallback();
    },

    /**
     * @param {function} callback
     * @return {boolean}
     */
    setExpandCallback: function(callback)
    {
        this._expandCallback = callback;
        var pending = this._expandPending;
        delete this._expandPending;
        return pending;
    },

    /**
     * @param {function} callback
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
    wasShown: function()
    {
        WebInspector.View.prototype.wasShown.call(this);
        for (var i = 0; i < this._panes.length; i++)
            this._attachToPane(i);
    },

    /**
     * @param {WebInspector.SidebarPane} pane
     */
    addPane: function(pane)
    {
        var index = this._panes.length; 
        this._panes.push(pane);
        this._addTitle(pane, index);
        if (this.isShowing())
            this._attachToPane(index);
    },

    /**
     * @param {WebInspector.SidebarPane} pane
     * @param {number} index
     */
    _addTitle: function(pane, index)
    {
        var title = this.element.createChild("div", "pane-title");
        title.textContent = pane.title();
        title.tabIndex = 0;
        title.addEventListener("click", this._togglePane.bind(this, index), false);
        title.addEventListener("keydown", this._onTitleKeyDown.bind(this, index), false);
        title.appendChild(pane.titleElement);
        this._titles.push(title);
    },

    /**
     * @param {number} index
     */
    _attachToPane: function(index)
    {
        var pane = this._panes[index];
        var title = this._titles[index];
        var expandPending = pane.setExpandCallback(this._onPaneExpanded.bind(this, index));
        this._setExpanded(index, this._isExpanded(index) || expandPending);
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
            this._panes[index].prepareContent(this._onPaneExpanded.bind(this, index));
        else
            this._collapsePane(index);
    },

    /**
     * @param {number} index
     */
    _onPaneExpanded: function(index)
    {
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
