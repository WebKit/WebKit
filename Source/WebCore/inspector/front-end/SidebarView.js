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
 * @extends {WebInspector.SplitView}
 * @param {string=} sidebarPosition
 * @param {string=} sidebarWidthSettingName
 * @param {number=} defaultSidebarWidth
 */
WebInspector.SidebarView = function(sidebarPosition, sidebarWidthSettingName, defaultSidebarWidth)
{
    WebInspector.SplitView.call(this, true, sidebarWidthSettingName, defaultSidebarWidth || 200);

    this._leftElement = this.firstElement();
    this._rightElement = this.secondElement();

    this._minimumSidebarWidth = Preferences.minSidebarWidth;
    this._minimumMainWidthPercent = 50;

    this._mainElementHidden = false;
    this._sidebarElementHidden = false;

    this._innerSetSidebarPosition(sidebarPosition || WebInspector.SidebarView.SidebarPosition.Left);
    this.setSecondIsSidebar(sidebarPosition !== WebInspector.SidebarView.SidebarPosition.Left);
}

WebInspector.SidebarView.EventTypes = {
    Resized: "Resized"
}

/**
 * @enum {string}
 */
WebInspector.SidebarView.SidebarPosition = {
    Left: "Left",
    Right: "Right"
}

WebInspector.SidebarView.prototype = {
    /**
     * @return {boolean}
     */
    _hasLeftSidebar: function()
    {
        return this._sidebarPosition === WebInspector.SidebarView.SidebarPosition.Left;
    },

    /**
     * @return {Element}
     */
    get mainElement()
    {
        return this._hasLeftSidebar() ? this._rightElement : this._leftElement;
    },

    /**
     * @return {Element}
     */
    get sidebarElement()
    {
        return this._hasLeftSidebar() ? this._leftElement : this._rightElement;
    },

    /**
     * @param {string} sidebarPosition
     */
    _innerSetSidebarPosition: function(sidebarPosition)
    {
        this._sidebarPosition = sidebarPosition;

        if (this._hasLeftSidebar()) {
            this._leftElement.addStyleClass("split-view-sidebar-left");
            this._rightElement.removeStyleClass("split-view-sidebar-right");
        } else {
            this._rightElement.addStyleClass("split-view-sidebar-right");
            this._leftElement.removeStyleClass("split-view-sidebar-left");
        }
    },

    /**
     * @param {number} width
     */
    setMinimumSidebarWidth: function(width)
    {
        this._minimumSidebarWidth = width;
    },

    /**
     * @param {number} widthPercent
     */
    setMinimumMainWidthPercent: function(widthPercent)
    {
        this._minimumMainWidthPercent = widthPercent;
    },

    /**
     * @param {number} width
     */
    setSidebarWidth: function(width)
    {
        this.setSidebarSize(width);
    },

    /**
     * @return {number}
     */
    sidebarWidth: function()
    {
        return this.sidebarSize();
    },

    onResize: function()
    {
        WebInspector.SplitView.prototype.onResize.call(this);
        this.dispatchEventToListeners(WebInspector.SidebarView.EventTypes.Resized, this.sidebarWidth());
    },

    /**
     * @param {number} size
     */
    applyConstraints: function(size)
    {
        return Number.constrain(size, this._minimumSidebarWidth, this.element.offsetWidth * (100 - this._minimumMainWidthPercent) / 100);
    },

    hideMainElement: function()
    {
        if (this._hasLeftSidebar())
            this.showOnlyFirst();
        else
            this.showOnlySecond();
    },

    showMainElement: function()
    {
        this.showBoth();
    },

    hideSidebarElement: function()
    {
        if (this._hasLeftSidebar())
            this.showOnlySecond();
        else
            this.showOnlyFirst();
    },

    showSidebarElement: function()
    {
        this.showBoth();
    },

    /**
     * @return {Array.<Element>}
     */
    elementsToRestoreScrollPositionsFor: function()
    {
        return [ this.mainElement, this.sidebarElement ];
    },

    __proto__: WebInspector.SplitView.prototype
}
