/*
 * Copyright (C) 2017 Apple Inc. All rights reserved.
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

WI.MediaPlayerTreeElement = class MediaPlayerTreeElement extends WI.FolderizedTreeElement
{
    constructor(representedObject)
    {
        console.assert(representedObject instanceof WI.MediaPlayer);

        let title = representedObject.originUrl.split('/').slice(-1)[0] || WI.UIString("Empty");
        let subtitle = representedObject.contentType || WI.UIString("Unknown Type");
        super("media", title, subtitle, representedObject);
    }

    // Public

    updateTitles()
    {
        this.mainTitle = this.representedObject.originUrl.split('/').slice(-1)[0] || WI.UIString("Empty");
        this.subtitle = this.representedObject.contentType || WI.UIString("Unknown Type");
    }

    // Protected

    onattach()
    {
        super.onattach();

        this.element.addEventListener("mouseover", this._handleMouseOver.bind(this));
        this.element.addEventListener("mouseout", this._handleMouseOut.bind(this));

        this.representedObject.addEventListener(WI.MediaPlayer.Event.PlayerDidPlay, this._handlePlay, this);
        this.representedObject.addEventListener(WI.MediaPlayer.Event.PlayerDidPause, this._handlePause, this);

        this.onpopulate();
    }

    ondetach()
    {
        this.element.removeEventListener("mouseover", this._handleMouseOver.bind(this));
        this.element.removeEventListener("mouseout", this._handleMouseOut.bind(this));

        this.iconElement.removeEventListener('click', this._handleIconClick.bind(this));

        this.representedObject.removeEventListener(WI.MediaPlayer.Event.PlayerDidPlay, this._handlePlay, this);
        this.representedObject.removeEventListener(WI.MediaPlayer.Event.PlayerDidPause, this._handlePause, this);

        super.ondetach();
    }

    onpopulate()
    {
        super.onpopulate();

        if (this.children.length && !this.shouldRefreshChildren)
            return;

        this.shouldRefreshChildren = false;

        this.removeChildren();

        this.iconElement.classList.add('paused');
        this.iconElement.addEventListener('click', this._handleIconClick.bind(this));
    }

    populateContextMenu(contextMenu, event)
    {
        super.populateContextMenu(contextMenu, event);

        contextMenu.appendItem(WI.UIString("Log MediaPlayer"), () => {
            WI.RemoteObject.resolveMediaElement(this.representedObject, WI.RuntimeManager.ConsoleObjectGroup, (remoteObject) => {
                if (!remoteObject)
                    return;

                const text = WI.UIString("Selected Media Player");
                const addSpecialUserLogClass = true;
                WI.consoleLogViewController.appendImmediateExecutionWithResult(text, remoteObject, addSpecialUserLogClass);
            });
        });

        contextMenu.appendSeparator();
    }

    // Private

    _handleItemAdded(event)
    {
        this.addChildForRepresentedObject(event.data.item);
    }

    _handleItemRemoved(event)
    {
        this.removeChildForRepresentedObject(event.data.item);
    }

    _handleMouseOver(event)
    {
        this.representedObject.requestNode().then((node) => {
            if (!node || !node.ownerDocument)
                return;
            node.highlight();
        });
    }

    _handleMouseOut(event)
    {
        WI.domManager.hideDOMNodeHighlight();
    }

    _handleIconClick(event)
    {
        const callback = (error) => {
            if (error) console.error(error);
        }
        if (this.iconElement.classList.contains('paused'))
            this.representedObject.play(callback);
        else
            this.representedObject.pause(callback);
    }

    _handlePlay()
    {
        this.iconElement.classList.remove('paused');
        this.iconElement.classList.add('playing');
    }

    _handlePause()
    {
        this.iconElement.classList.remove('playing');
        this.iconElement.classList.add('paused');
    }

    _updateStatus()
    {
    }
};
