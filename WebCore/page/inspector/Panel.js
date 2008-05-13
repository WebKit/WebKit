/*
 * Copyright (C) 2007, 2008 Apple Inc.  All rights reserved.
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

WebInspector.Panel = function()
{
    WebInspector.View.call(this);

    this.element.addStyleClass("panel");
}

WebInspector.Panel.prototype = {
    get toolbarItem()
    {
        if (this._toolbarItem)
            return this._toolbarItem;

        // Sample toolbar item as markup:
        // <button class="toolbar-item resources toggleable">
        // <img class="toolbar-icon">
        // <div class="toolbar-label">Resources</div>
        // </button>

        this._toolbarItem = document.createElement("button");
        this._toolbarItem.className = "toolbar-item toggleable";
        this._toolbarItem.panel = this;

        if ("toolbarItemClass" in this)
            this._toolbarItem.addStyleClass(this.toolbarItemClass);

        var iconElement = document.createElement("img");
        iconElement.className = "toolbar-icon";
        this._toolbarItem.appendChild(iconElement);

        if ("toolbarItemLabel" in this) {
            var labelElement = document.createElement("div");
            labelElement.className = "toolbar-label";
            labelElement.textContent = this.toolbarItemLabel;
            this._toolbarItem.appendChild(labelElement);
        }

        return this._toolbarItem;
    },

    show: function()
    {
        WebInspector.View.prototype.show.call(this);

        var statusBarItems = this.statusBarItems;
        if (statusBarItems) {
            this._statusBarItemContainer = document.createElement("div");
            for (var i = 0; i < statusBarItems.length; ++i)
                this._statusBarItemContainer.appendChild(statusBarItems[i]);
            document.getElementById("main-status-bar").appendChild(this._statusBarItemContainer);
        }

        if ("_toolbarItem" in this)
            this._toolbarItem.addStyleClass("toggled-on");

        WebInspector.currentFocusElement = document.getElementById("main-panels");
    },

    hide: function()
    {
        WebInspector.View.prototype.hide.call(this);

        if (this._statusBarItemContainer && this._statusBarItemContainer.parentNode)
            this._statusBarItemContainer.parentNode.removeChild(this._statusBarItemContainer);
        delete this._statusBarItemContainer;
        if ("_toolbarItem" in this)
            this._toolbarItem.removeStyleClass("toggled-on");
    },

    attach: function()
    {
        if (!this.element.parentNode)
            document.getElementById("main-panels").appendChild(this.element);
    }
}

WebInspector.Panel.prototype.__proto__ = WebInspector.View.prototype;
