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

WebInspector.HoverMenu = function(delegate)
{
    WebInspector.Object.call(this);

    this.delegate = delegate;

    this._element = document.createElement("div");
    this._element.className = WebInspector.HoverMenu.StyleClassName;
    this._element.addEventListener("transitionend", this, true);

    this._button = this._element.appendChild(document.createElement("img"));
    this._button.addEventListener("click", this);
}

WebInspector.HoverMenu.StyleClassName = "hover-menu";
WebInspector.HoverMenu.VisibleClassName = "visible";

WebInspector.HoverMenu.prototype = {
    constructor: WebInspector.HoverMenu,
    __proto__: WebInspector.Object.prototype,

    // Public

    get element()
    {
        return this._element;
    },

    present: function(targetFrame)
    {
        var style = this._element.style;
        style.left = Math.ceil(targetFrame.origin.x) + "px";
        style.top = Math.ceil(targetFrame.origin.y) + "px";
        style.width = Math.ceil(targetFrame.size.width) + "px";
        style.height = Math.ceil(targetFrame.size.height) + "px";

        var element = this._element;
        document.body.appendChild(element);
        setTimeout(function() {
            element.classList.add(WebInspector.HoverMenu.VisibleClassName);
        });

        window.addEventListener("scroll", this, true);
    },

    dismiss: function()
    {
        if (this._element.parentNode !== document.body)
            return;

        this._element.classList.remove(WebInspector.HoverMenu.VisibleClassName);

        window.removeEventListener("scroll", this, true);
    },

    // Protected

    handleEvent: function(event)
    {
        switch (event.type) {
        case "scroll":
            if (!this._element.contains(event.target))
                this.dismiss();
            break;
        case "click":
            this._handleClickEvent(event);
            break;
        case "transitionend":
            if (!this._element.classList.contains(WebInspector.HoverMenu.VisibleClassName))
                this._element.remove();
            break;
        }
    },

    // Private

    _handleClickEvent: function(event)
    {
        if (this.delegate && typeof this.delegate.hoverMenuButtonWasPressed === "function")
            this.delegate.hoverMenuButtonWasPressed(this);
    }
}
