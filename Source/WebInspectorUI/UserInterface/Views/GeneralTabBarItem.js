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
    constructor(image, title, isEphemeral = false)
    {
        super(image, title);

        this._isEphemeral = isEphemeral;

        if (this._isEphemeral) {
            this.element.classList.add("ephemeral");

            let closeButtonElement = document.createElement("div");
            closeButtonElement.classList.add(WI.TabBarItem.CloseButtonStyleClassName);
            closeButtonElement.title = WI.UIString("Click to close this tab");

            this.element.insertBefore(closeButtonElement, this.element.firstChild);
            this.element.addEventListener("contextmenu", this._handleContextMenuEvent.bind(this));
        }
    }

    static fromTabInfo({image, title, isEphemeral})
    {
        return new WI.GeneralTabBarItem(image, title, isEphemeral);
    }

    // Public

    get isEphemeral() { return this._isEphemeral; }

    get title()
    {
        return super.title;
    }

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

        let contextMenu = WI.ContextMenu.createFromEvent(event);
        contextMenu.appendItem(WI.UIString("Close Tab"), () => {
            this._parentTabBar.removeTabBarItem(this);
        }, this.isDefaultTab);
        contextMenu.appendSeparator();
    }
};
