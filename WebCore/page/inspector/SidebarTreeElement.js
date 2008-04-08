/*
 * Copyright (C) 2008 Apple Inc. All Rights Reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

WebInspector.SidebarSectionTreeElement = function(title, representedObject, hasChildren)
{
    TreeElement.call(this, title.escapeHTML(), representedObject || {}, hasChildren);
}

WebInspector.SidebarSectionTreeElement.prototype = {
    selectable: false,

    onattach: function()
    {
        this._listItemNode.addStyleClass("sidebar-tree-section");
    },

    onreveal: function()
    {
        if (this.listItemElement)
            this.listItemElement.scrollIntoViewIfNeeded(false);
    }
}

WebInspector.SidebarSectionTreeElement.prototype.__proto__ = TreeElement.prototype;

WebInspector.SidebarTreeElement = function(className, title, subtitle, representedObject, hasChildren)
{
    TreeElement.call(this, "", representedObject || {}, hasChildren);

    if (hasChildren) {
        this.disclosureButton = document.createElement("button");
        this.disclosureButton.className = "disclosure-button";
    }

    this.titlesElement = document.createElement("div");
    this.titlesElement.className = "titles";

    this.titleElement = document.createElement("span");
    this.titleElement.className = "title";
    this.titlesElement.appendChild(this.titleElement);

    this.subtitleElement = document.createElement("span");
    this.subtitleElement.className = "subtitle";
    this.titlesElement.appendChild(this.subtitleElement);

    this.className = className;
    this.mainTitle = title;
    this.subtitle = subtitle;
}

WebInspector.SidebarTreeElement.prototype = {
    get mainTitle()
    {
        return this._mainTitle;
    },

    set mainTitle(x)
    {
        this._mainTitle = x;
        this.refreshTitles();
    },

    get subtitle()
    {
        return this._subtitle;
    },

    set subtitle(x)
    {
        this._subtitle = x;
        this.refreshTitles();
    },

    refreshTitles: function()
    {
        var mainTitle = this.mainTitle;
        if (this.titleElement.textContent !== mainTitle)
            this.titleElement.textContent = mainTitle;

        var subtitle = this.subtitle;
        if (subtitle) {
            if (this.subtitleElement.textContent !== subtitle)
                this.subtitleElement.textContent = subtitle;
            this.titlesElement.removeStyleClass("no-subtitle");
        } else
            this.titlesElement.addStyleClass("no-subtitle");
    },

    isEventWithinDisclosureTriangle: function(event)
    {
        return event.target === this.disclosureButton;
    },

    onattach: function()
    {
        this._listItemNode.addStyleClass("sidebar-tree-item");

        if (this.className)
            this._listItemNode.addStyleClass(this.className);

        if (this.hasChildren && this.disclosureButton)
            this._listItemNode.appendChild(this.disclosureButton);

        var iconElement = document.createElement("img");
        iconElement.className = "icon";
        this._listItemNode.appendChild(iconElement);

        this._listItemNode.appendChild(this.titlesElement);
    },

    onreveal: function()
    {
        if (this._listItemNode)
            this._listItemNode.scrollIntoViewIfNeeded(false);
    }
}

WebInspector.SidebarTreeElement.prototype.__proto__ = TreeElement.prototype;
