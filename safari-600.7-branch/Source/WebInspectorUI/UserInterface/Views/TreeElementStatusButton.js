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

WebInspector.TreeElementStatusButton = function(element) {
    WebInspector.Object.call(this);

    console.assert(element);

    this._element = element;
    this._element.classList.add(WebInspector.TreeElementStatusButton.StyleClassName);
    this._element.addEventListener("click", this._clicked.bind(this));
};

WebInspector.TreeElementStatusButton.StyleClassName = "status-button";
WebInspector.TreeElementStatusButton.DisabledStyleClassName = "disabled";

WebInspector.TreeElementStatusButton.Event = {
    Clicked: "status-button-clicked"
};

WebInspector.TreeElementStatusButton.prototype = {
    constructor: WebInspector.TreeElementStatusButton,

    // Public

    get element()
    {
        return this._element;
    },

    get hidden()
    {
        return !this._element.classList.contains(WebInspector.TreeElementStatusButton.DisabledStyleClassName);
    },

    set hidden(flag)
    {
        if (flag)
            this._element.classList.remove("hidden");
        else
            this._element.classList.add("hidden");
    },

    get enabled()
    {
        return !this._element.classList.contains(WebInspector.TreeElementStatusButton.DisabledStyleClassName);
    },

    set enabled(flag)
    {
        if (flag)
            this._element.classList.remove(WebInspector.TreeElementStatusButton.DisabledStyleClassName);
        else
            this._element.classList.add(WebInspector.TreeElementStatusButton.DisabledStyleClassName);
    },

    // Private

    _clicked: function(event)
    {
        if (!this.enabled)
            return;

        event.stopPropagation();

        this.dispatchEventToListeners(WebInspector.TreeElementStatusButton.Event.Clicked, event);
    }
};

WebInspector.TreeElementStatusButton.prototype.__proto__ = WebInspector.Object.prototype;
