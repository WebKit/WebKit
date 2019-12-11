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

StatusLineView = function(message, status, label, repeatCount, url)
{
    BaseObject.call(this);

    console.assert(message);

    this.element = document.createElement("div");
    this.element.classList.add("status-line");
    this.element.__statusLineView = this;

    if (url) {
        this._statusBubbleElement = document.createElement("a");
        this._statusBubbleElement.href = url;
        this._statusBubbleElement.target = "_blank";
    } else
        this._statusBubbleElement = document.createElement("div");
    this._statusBubbleElement.classList.add("bubble");
    if (status != StatusLineView.Status.NoBubble)
        this.element.appendChild(this._statusBubbleElement);

    this._labelElement = document.createElement("div");
    this._labelElement.classList.add("label");

    this._messageElement = document.createElement("div");
    this._messageElement.classList.add("message");
    this.element.appendChild(this._messageElement);

    this.status = status || StatusLineView.Status.Neutral;
    this.message = message || "";
    this.label = label || "";
    this.repeatCount = repeatCount || 0;
    this.url = url || null;
};

StatusLineView.Status = {
    NoBubble: "no-bubble",
    Neutral: "neutral",
    Good: "good",
    Danger: "danger",
    Bad: "bad",
    Unauthorized: "unauthorized"
};

BaseObject.addConstructorFunctions(StatusLineView);

StatusLineView.prototype = {
    constructor: StatusLineView,
    __proto__: BaseObject.prototype,

    get status()
    {
        return this._status;
    },

    set status(x)
    {
        console.assert(x);
        if (!x)
            return;

        this.element.classList.remove(this._status);

        this._status = x;

        this.element.classList.add(this._status);
    },

    get repeatCount()
    {
        return this._repeatCount;
    },

    set repeatCount(x)
    {
        this._repeatCount = x;

        if (this._repeatCount) {
            this._statusBubbleElement.textContent = this._repeatCount;
            this._statusBubbleElement.classList.remove("pictogram");
        } else {
            this._statusBubbleElement.classList.add("pictogram");

            switch (this._status) {
            case StatusLineView.Status.Neutral:
                this._statusBubbleElement.textContent = "?";
                break;

            case StatusLineView.Status.Good:
                this._statusBubbleElement.textContent = "";
                break;

            case StatusLineView.Status.Danger:
                this._statusBubbleElement.textContent = "!";
                break;

            case StatusLineView.Status.Bad:
                this._statusBubbleElement.textContent = "\u2715";
                break;
            }
        }
    },

    get label()
    {
        return this._labelElement.textContent;
    },

    set label(x)
    {
        if (x) {
            this._labelElement.textContent = x;
            this.element.insertBefore(this._labelElement, this._messageElement);
            this.element.classList.remove("no-label");
        } else {
            this._labelElement.removeChildren();
            this._labelElement.remove();
            this.element.classList.add("no-label");
        }
    },

    get message()
    {
        return this._messageElement.textContent;
    },

    set message(x)
    {
        if (x instanceof Node) {
            this._messageElement.removeChildren();
            this._messageElement.appendChild(x);
        } else {
            this._messageElement.textContent = x;
        }
    },

    get statusBubbleElement()
    {
        return this._statusBubbleElement;
    }
};
