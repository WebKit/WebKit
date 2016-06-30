/*
 * Copyright (C) 2015 Apple Inc. All rights reserved.
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

WebInspector.TabBarItem = class TabBarItem extends WebInspector.Object
{
    constructor(image, title, pinned, representedObject)
    {
        super();

        this._parentTabBar = null;

        this._element = document.createElement("div");
        this._element.classList.add(WebInspector.TabBarItem.StyleClassName);
        this._element.setAttribute("role", "tab");
        this._element.tabIndex = 0;
        if (pinned)
            this._element.classList.add("pinned");
        this._element[WebInspector.TabBarItem.ElementReferenceSymbol] = this;

        if (!pinned) {
            this._closeButtonElement = document.createElement("div");
            this._closeButtonElement.classList.add(WebInspector.TabBarItem.CloseButtonStyleClassName);
            this._closeButtonElement.title = WebInspector.UIString("Click to close this tab");
            this._element.appendChild(this._closeButtonElement);

            var flexSpaceElement = document.createElement("div");
            flexSpaceElement.classList.add("flex-space");
            this._element.appendChild(flexSpaceElement);

            this._element.addEventListener("contextmenu", this._handleContextMenuEvent.bind(this));
        }

        this._iconElement = document.createElement("img");
        this._iconElement.classList.add("icon");
        this._element.appendChild(this._iconElement);

        if (!pinned) {
            var flexSpaceElement = document.createElement("div");
            flexSpaceElement.classList.add("flex-space");
            this._element.appendChild(flexSpaceElement);
        }

        this.title = title;
        this.image = image;
        this.representedObject = representedObject;
    }

    // Public

    get element()
    {
        return this._element;
    }

    get representedObject()
    {
        return this._representedObject;
    }

    set representedObject(representedObject)
    {
        this._representedObject = representedObject || null;
    }

    get parentTabBar()
    {
        return this._parentTabBar;
    }

    set parentTabBar(tabBar)
    {
        this._parentTabBar = tabBar || null;
    }

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

    get pinned()
    {
        return this._element.classList.contains("pinned");
    }

    get image()
    {
        return this._iconElement.src;
    }

    set image(url)
    {
        this._iconElement.src = url || "";
    }

    get title()
    {
        return this._element.title || "";
    }

    set title(title)
    {
        if (title && !this.pinned) {
            this._titleElement = document.createElement("span");
            this._titleElement.classList.add("title");

            this._titleContentElement = document.createElement("span");
            this._titleContentElement.classList.add("content");
            this._titleElement.appendChild(this._titleContentElement);

            this._titleContentElement.textContent = title;

            this._element.insertBefore(this._titleElement, this._element.lastChild);
        } else {
            if (this._titleElement)
                this._titleElement.remove();

            this._titleContentElement = null;
            this._titleElement = null;
        }

        this._element.title = title || "";
    }

    // Private

    _handleContextMenuEvent(event)
    {
        if (!this._parentTabBar)
            return;

        let closeTab = () => {
            this._parentTabBar.removeTabBarItem(this);
        };

        let closeOtherTabs = () => {
            let tabBarItems = this._parentTabBar.tabBarItems;
            for (let i = tabBarItems.length - 1; i >= 0; --i) {
                let item = tabBarItems[i];
                if (item === this || item.pinned)
                    continue;
                this._parentTabBar.removeTabBarItem(item);
            }
        };

        let hasOtherNonPinnedTabs = this._parentTabBar.tabBarItems.some((item) => item !== this && !item.pinned);
        let contextMenu = WebInspector.ContextMenu.createFromEvent(event);
        contextMenu.appendItem(WebInspector.UIString("Close Tab"), closeTab, this.isDefaultTab);
        contextMenu.appendItem(WebInspector.UIString("Close Other Tabs"), closeOtherTabs, !hasOtherNonPinnedTabs);
    }
};

WebInspector.TabBarItem.StyleClassName = "item";
WebInspector.TabBarItem.CloseButtonStyleClassName = "close";
WebInspector.TabBarItem.ElementReferenceSymbol = Symbol("tab-bar-item");
