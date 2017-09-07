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

WI.GeneralTabBarItem = class GeneralTabBarItem extends WI.TabBarItem
{
    constructor(image, title, representedObject)
    {
        super(image, title, representedObject);

        let closeButtonElement = document.createElement("div");
        closeButtonElement.classList.add(WI.TabBarItem.CloseButtonStyleClassName);
        closeButtonElement.title = WI.UIString("Click to close this tab; Option-click to close all tabs except this one");
        this.element.insertBefore(closeButtonElement, this.element.firstChild);

        this.element.addEventListener("contextmenu", this._handleContextMenuEvent.bind(this));
    }

    // Public

    set title(title)
    {
        if (title) {
            this._titleElement = document.createElement("span");
            this._titleElement.classList.add("title");

            let titleContentElement = document.createElement("span");
            titleContentElement.classList.add("content");
            titleContentElement.textContent = title;
            this._titleElement.appendChild(titleContentElement);

            this.element.insertBefore(this._titleElement, this.element.lastChild);
        } else {
            if (this._titleElement)
                this._titleElement.remove();

            this._titleElement = null;
        }

        super.title = title;
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
                if (item === this || item instanceof WI.PinnedTabBarItem)
                    continue;
                this._parentTabBar.removeTabBarItem(item);
            }
        };

        let hasOtherNonPinnedTabs = this._parentTabBar.tabBarItems.some((item) => item !== this && !(item instanceof WI.PinnedTabBarItem));
        let contextMenu = WI.ContextMenu.createFromEvent(event);
        contextMenu.appendItem(WI.UIString("Close Tab"), closeTab, this.isDefaultTab);
        contextMenu.appendItem(WI.UIString("Close Other Tabs"), closeOtherTabs, !hasOtherNonPinnedTabs);
    }
};
