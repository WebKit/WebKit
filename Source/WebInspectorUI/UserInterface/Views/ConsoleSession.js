/*
 * Copyright (C) 2015 Apple Inc. All rights reserved.
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
 * 3.  Neither the name of Apple Inc. ("Apple") nor the names of
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

WI.ConsoleSession = class ConsoleSession extends WI.Object
{
    constructor(data)
    {
        super();

        this._hasMessages = false;

        let element = document.createElement("div");
        element.className = "console-session";

        let header = document.createElement("div");
        header.classList.add("console-session-header");

        let timestampText = (new Date(data.timestamp || Date.now())).toLocaleTimeString();

        switch (data.newSessionReason) {
        case WI.ConsoleSession.NewSessionReason.PageReloaded:
            header.textContent = WI.UIString("Page reloaded at %s").format(timestampText);
            break;
        case WI.ConsoleSession.NewSessionReason.PageNavigated:
            header.textContent = WI.UIString("Page navigated to %s at %s").format(WI.networkManager.mainFrame.url.truncateMiddle(100), timestampText);
            break;
        case WI.ConsoleSession.NewSessionReason.ConsoleCleared:
            header.textContent = WI.UIString("Console cleared at %s").format(timestampText);
            break;
        default:
            header.textContent = WI.UIString("Console opened at %s").format(timestampText);
            break;
        }
        
        element.append(header);
        this.element = element;
        this._messagesElement = element;
    }

    // Public

    addMessageView(messageView)
    {
        var messageElement = messageView.element;
        messageElement.classList.add(WI.LogContentView.ItemWrapperStyleClassName);
        this.append(messageElement);
    }

    append(messageOrGroupElement)
    {
        this._hasMessages = true;
        this._messagesElement.append(messageOrGroupElement);
    }

    hasMessages()
    {
        return this._hasMessages;
    }
};

WI.ConsoleSession.NewSessionReason = {
    PageReloaded: Symbol("Page reloaded"),
    PageNavigated: Symbol("Page navigated"),
    ConsoleCleared: Symbol("Console cleared"),
};
