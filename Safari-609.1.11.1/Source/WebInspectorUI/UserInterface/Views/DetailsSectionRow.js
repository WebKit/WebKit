/*
 * Copyright (C) 2013 Apple Inc. All rights reserved.
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

WI.DetailsSectionRow = class DetailsSectionRow extends WI.Object
{
    constructor(emptyMessage)
    {
        super();

        this._element = document.createElement("div");
        this._element.className = "row";

        this._emptyMessage = emptyMessage || "";
    }

    // Public

    get element()
    {
        return this._element;
    }

    get emptyMessage()
    {
        return this._emptyMessage;
    }

    set emptyMessage(emptyMessage)
    {
        this._emptyMessage = emptyMessage || "";

        if (!this._element.childNodes.length)
            this.showEmptyMessage();
    }

    showEmptyMessage()
    {
        this.element.classList.add(WI.DetailsSectionRow.EmptyStyleClassName);

        if (this._emptyMessage instanceof Node) {
            this.element.removeChildren();
            this.element.appendChild(this._emptyMessage);
        } else
            this.element.textContent = this._emptyMessage;
    }

    hideEmptyMessage()
    {
        this.element.classList.remove(WI.DetailsSectionRow.EmptyStyleClassName);
        this.element.removeChildren();
    }
};

WI.DetailsSectionRow.EmptyStyleClassName = "empty";
