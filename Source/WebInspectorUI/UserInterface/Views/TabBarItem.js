/*
 * Copyright (C) 2015 Apple Inc. All rights reserved.
 * Copyright (C) 2016 Devin Rousso <webkit@devinrousso.com>. All rights reserved.
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

WI.TabBarItem = class TabBarItem extends WI.Object
{
    constructor(image, title, representedObject)
    {
        super();

        this._parentTabBar = null;

        this._element = document.createElement("div");
        this._element.classList.add(WI.TabBarItem.StyleClassName);
        this._element.setAttribute("role", "tab");
        this._element.tabIndex = 0;
        this._element[WI.TabBarItem.ElementReferenceSymbol] = this;

        this._element.createChild("div", "flex-space");

        this._iconElement = document.createElement("img");
        this._iconElement.classList.add("icon");
        this._element.appendChild(this._iconElement);

        this._element.createChild("div", "flex-space");

        this.title = title;
        this.image = image;
        this.representedObject = representedObject;
    }

    // Public

    get element() { return this._element; }

    get representedObject() { return this._representedObject; }
    set representedObject(representedObject) { this._representedObject = representedObject || null; }

    get parentTabBar() { return this._parentTabBar; }
    set parentTabBar(tabBar){ this._parentTabBar = tabBar || null; }

    get selected()
    {
        return this._element.classList.contains("selected");
    }

    set selected(selected)
    {
        this._element.classList.toggle("selected", selected);

        if (selected)
            this._element.setAttribute("aria-selected", "true");
        else
            this._element.removeAttribute("aria-selected");
    }

    get disabled()
    {
        return this._element.classList.contains("disabled");
    }

    set disabled(disabled)
    {
        this._element.classList.toggle("disabled", disabled);
    }

    get isDefaultTab()
    {
        return this._element.classList.contains("default-tab");
    }

    set isDefaultTab(isDefaultTab)
    {
        this._element.classList.toggle("default-tab", isDefaultTab);
    }

    get image() { return this._iconElement.src; }
    set image(url) { this._iconElement.src = url || ""; }

    get title()
    {
        return this._title;
    }

    set title(title)
    {
        title = title || "";
        if (this._title === title)
            return;

        this._title = title;
        this.titleDidChange();
    }

    titleDidChange()
    {
        // Implemented by subclasses.
    }
};

WI.TabBarItem.StyleClassName = "item";
WI.TabBarItem.CloseButtonStyleClassName = "close";
WI.TabBarItem.ElementReferenceSymbol = Symbol("tab-bar-item");
