/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
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
 * @extends {WebInspector.View}
 * @param {boolean} isVertical
 * @param {string=} sidebarSizeSettingName
 * @param {number=} defaultSidebarSize
 */
WebInspector.SplitView = function(isVertical, sidebarSizeSettingName, defaultSidebarSize)
{
    WebInspector.View.call(this);
    this._isVertical = isVertical;

    this.registerRequiredCSS("splitView.css");

    this.element.className = "split-view";

    this._firstElement = document.createElement("div");
    this._firstElement.className = "split-view-contents split-view-contents-" + (isVertical ? "vertical" : "horizontal");
    if (isVertical)
        this._firstElement.style.left = 0;
    else
        this._firstElement.style.top = 0;
    this.element.appendChild(this._firstElement);

    this._secondElement = document.createElement("div");
    this._secondElement.className = "split-view-contents split-view-contents-" + (isVertical ? "vertical" : "horizontal");
    if (isVertical)
        this._secondElement.style.right = 0;
    else
        this._secondElement.style.bottom = 0;
    this.element.appendChild(this._secondElement);

    this._resizerElement = document.createElement("div");
    this._resizerElement.className = "split-view-resizer split-view-resizer-" + (isVertical ? "vertical" : "horizontal");
    this.installResizer(this._resizerElement);
    this._resizable = true;
    this.element.appendChild(this._resizerElement);

    defaultSidebarSize = defaultSidebarSize || 200;
    this._savedSidebarSize = defaultSidebarSize;

    this._sidebarSizeSettingName = sidebarSizeSettingName;
    if (this._sidebarSizeSettingName)
        WebInspector.settings[this._sidebarSizeSettingName] = WebInspector.settings.createSetting(this._sidebarSizeSettingName, undefined);

    this._secondIsSidebar = true;
}

WebInspector.SplitView.prototype = {
    /**
     * @return {Element}
     */
    firstElement: function()
    {
        return this._firstElement;
    },

    /**
     * @return {Element}
     */
    secondElement: function()
    {
        return this._secondElement;
    },

    /**
     * @param {boolean} secondIsSidebar
     */
    setSecondIsSidebar: function(secondIsSidebar)
    {
        this._secondIsSidebar = secondIsSidebar;
    },

    /**
     * @return {Element}
     */
    resizerElement: function()
    {
        return this._resizerElement;
    },

    showOnlyFirst: function()
    {
        this._showOnly(this._firstElement, this._secondElement);
    },

    showOnlySecond: function()
    {
        this._showOnly(this._secondElement, this._firstElement);
    },

    /**
     * @param {Element} sideA
     * @param {Element} sideB
     */
    _showOnly: function(sideA, sideB)
    {
        sideA.removeStyleClass("hidden");
        sideA.addStyleClass("maximized");
        sideB.addStyleClass("hidden");
        sideB.removeStyleClass("maximized");
        this._removeAllLayoutProperties();

        this._firstElement.style.right = 0;
        this._firstElement.style.bottom = 0;
        this._firstElement.style.width = "100%";
        this._firstElement.style.height = "100%";

        this._secondElement.style.left = 0;
        this._secondElement.style.top = 0;
        this._secondElement.style.width = "100%";
        this._secondElement.style.height = "100%";

        this._isShowingOne = true;
        this.setResizable(false);
        this.doResize();
    },

    _removeAllLayoutProperties: function()
    {
        this._firstElement.style.removeProperty("right");
        this._firstElement.style.removeProperty("bottom");
        this._firstElement.style.removeProperty("width");
        this._firstElement.style.removeProperty("height");
  
        this._secondElement.style.removeProperty("left");
        this._secondElement.style.removeProperty("top");
        this._secondElement.style.removeProperty("width");
        this._secondElement.style.removeProperty("height");
    },

    showBoth: function()
    {
        this._isShowingOne = false;
        this._firstElement.removeStyleClass("hidden");
        this._firstElement.removeStyleClass("maximized");
        this._secondElement.removeStyleClass("hidden");
        this._secondElement.removeStyleClass("maximized");

        // Force update
        delete this._sidebarSize;
        this.setSidebarSize(this._lastSidebarSize());

        this.setResizable(true);
        this.doResize();
    },

    /**
     * @param {boolean} resizable
     */
    setResizable: function(resizable)
    {
        if (this._resizable === resizable)
            return;
        this._resizable = resizable;
        if (resizable)
            this._resizerElement.removeStyleClass("hidden");
        else
            this._resizerElement.addStyleClass("hidden");
    },

    /**
     * @param {number} size
     */
    setSidebarSize: function(size)
    {
        if (this._sidebarSize === size)
            return;

        size = this.applyConstraints(size);
        this._innerSetSidebarSize(size);
        this._saveSidebarSize(size);
    },

    /**
     * @return {number}
     */
    sidebarSize: function()
    {
        return this._sidebarSize;
    },

    /**
     * @return {number}
     */
    totalSize: function()
    {
        return this._totalSize;
    },

    /**
     * @param {number} size
     */
    _innerSetSidebarSize: function(size)
    {
        if (this._isShowingOne)
            return;

        this._removeAllLayoutProperties();

        if (this._isVertical) {
            var resizerWidth = this._resizerElement.offsetWidth;
            if (this._secondIsSidebar) {
                this._firstElement.style.right = size + "px";
                this._secondElement.style.width = size + "px";
                this._resizerElement.style.right = size - resizerWidth / 2 + "px";
            } else {
                this._firstElement.style.width = size + "px";;
                this._secondElement.style.left = size + "px";;
                this._resizerElement.style.left = size - resizerWidth / 2 + "px";
            }
        } else {
            var resizerHeight = this._resizerElement.offsetHeight;
           
            if (this._secondIsSidebar) {
                this._firstElement.style.bottom = size + "px";;
                this._secondElement.style.height = size + "px";;
                this._resizerElement.style.bottom = size - resizerHeight / 2 + "px";
            } else {
                this._firstElement.style.height = size + "px";;
                this._secondElement.style.top = size + "px";;
                this._resizerElement.style.top = size - resizerHeight / 2 + "px";
            }
        }

        this._sidebarSize = size;
        this.doResize();
    },

    /**
     * @param {number} size
     * @return {number}
     */
    applyConstraints: function(size)
    {
        const minSize = 20;
        size = Math.max(size, minSize);
        if (this._totalSize - size < minSize)
            size = this._totalSize - minSize;
        return size;
    },

    wasShown: function()
    {
        this._totalSize = this._isVertical ? this.element.offsetWidth : this.element.offsetHeight;
        this.setSidebarSize(this._lastSidebarSize());
    },

    onResize: function()
    {
        var oldTotalSize = this._totalSize;
        this._totalSize = this._isVertical ? this.element.offsetWidth : this.element.offsetHeight;
    },

    /**
     * @param {Event} event
     * @return {boolean}
     */
    _startResizerDragging: function(event)
    {
        if (!this._resizable)
            return false;

        this._dragOffset = (this._secondIsSidebar ? this._totalSize - this._sidebarSize : this._sidebarSize) - (this._isVertical ? event.pageX : event.pageY);
        return true;
    },

    /**
     * @param {Event} event
     */
    _resizerDragging: function(event)
    {
        var newOffset = (this._isVertical ? event.pageX : event.pageY) + this._dragOffset;
        var newSize = (this._secondIsSidebar ? this._totalSize - newOffset : newOffset);
        this.setSidebarSize(newSize);
        event.preventDefault();
    },

    /**
     * @param {Event} event
     */
    _endResizerDragging: function(event)
    {
        delete this._dragOffset;
    },

    /**
     * @param {Element} resizerElement
     */
    installResizer: function(resizerElement)
    {
        WebInspector.installDragHandle(resizerElement, this._startResizerDragging.bind(this), this._resizerDragging.bind(this), this._endResizerDragging.bind(this), this._isVertical ? "ew-resize" : "ns-resize");
    },

    /**
     * @return {number}
     */
    _lastSidebarSize: function()
    {
        return this._sidebarSizeSettingName ? WebInspector.settings[this._sidebarSizeSettingName].get() || this._savedSidebarSize : this._savedSidebarSize;
    },

    /**
     * @param {number} size
     */
    _saveSidebarSize: function(size)
    {
        this._savedSidebarSize = size;
        if (!this._sidebarSizeSettingName)
            return;

        WebInspector.settings[this._sidebarSizeSettingName].set(this._savedSidebarSize);
    },

    __proto__: WebInspector.View.prototype
}
