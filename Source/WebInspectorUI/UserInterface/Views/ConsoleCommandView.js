/*
 * Copyright (C) 2007, 2008, 2013 Apple Inc.  All rights reserved.
 * Copyright (C) 2009 Joseph Pecoraro
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

WI.ConsoleCommandView = class ConsoleCommandView
{
    constructor(commandText, className)
    {
        this._commandText = commandText;
        this._className = className || "";
    }

    // Public

    render()
    {
        this._element = document.createElement("div");
        this._element.classList.add("console-user-command");
        this._element.dir = "ltr";
        this._element.setAttribute("data-labelprefix", WI.UIString("Input: "));

        if (this._className)
            this._element.classList.add(this._className);

        this._formattedCommandElement = this._element.appendChild(document.createElement("span"));
        this._formattedCommandElement.classList.add("console-message-body");
        this._formattedCommandElement.textContent = this._commandText;

        // FIXME: <https://webkit.org/b/143545> Web Inspector: LogContentView should use higher level objects
        this._element.__commandView = this;
    }

    get element()
    {
        return this._element;
    }

    get commandText()
    {
        return this._commandText;
    }

    toClipboardString(isPrefixOptional)
    {
        return (isPrefixOptional ? "" : "> ") + this._commandText.removeWordBreakCharacters();
    }
};
