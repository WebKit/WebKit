/*
 * Copyright (C) 2009 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/**
 * This class provides a popup that can be shown relative to an anchor element
 * or at an arbitrary absolute position.
 * Points are Objects: {x: xValue, y: yValue}.
 * Rectangles are Objects: {x: xValue, y: yValue, width: widthValue, height: heightValue}.
 *
 * element is an optional unparented visible element (style.display != "none" AND style.visibility != "hidden").
 * If the element is absent/undefined, it must have been set with the element(x) setter before the show() method invocation.
 */
WebInspector.Popup = function(element)
{
    if (element)
        this.element = element;
    this._keyHandler = this._keyEventHandler.bind(this);
    this._mouseDownHandler = this._mouseDownEventHandler.bind(this);
    this._visible = false;
    this._autoHide = true;
}

WebInspector.Popup.prototype = {
    show: function()
    {
        if (this.visible)
            return;
        var ownerDocument = this._contentElement.ownerDocument;
        if (!ownerDocument)
            return;

        this._glasspaneElement = ownerDocument.createElement("div");
        this._glasspaneElement.className = "popup-glasspane";
        ownerDocument.body.appendChild(this._glasspaneElement);

        this._contentElement.positionAt(0, 0);
        this._contentElement.removeStyleClass("hidden");
        ownerDocument.body.appendChild(this._contentElement);

        this.positionElement();
        this._visible = true;
        ownerDocument.addEventListener("keydown", this._keyHandler, false);
        ownerDocument.addEventListener("mousedown", this._mouseDownHandler, false);
    },

    hide: function()
    {
        if (this.visible) {
            this._visible = false;
            this._contentElement.ownerDocument.removeEventListener("keydown", this._keyHandler, false);
            this._contentElement.ownerDocument.removeEventListener("mousedown", this._mouseDownHandler, false);
            this._glasspaneElement.parentElement.removeChild(this._glasspaneElement);
            this._contentElement.parentElement.removeChild(this._contentElement);
        }
    },

    get visible()
    {
        return this._visible;
    },

    set element(x)
    {
        this._checkNotVisible();
        this._contentElement = x;
        this._contentElement.addStyleClass("hidden");
    },

    get element()
    {
        return this._contentElement;
    },

    positionElement: function()
    {
        var element = this._contentElement;
        var anchorElement = this._anchorElement;

        var targetDocument = element.ownerDocument;
        var targetDocumentBody = targetDocument.body;
        var targetDocumentElement = targetDocument.documentElement;
        var clippingBox = {x: 0, y: 0, width: targetDocumentElement.clientWidth, height: targetDocumentElement.clientHeight};
        var parentElement = element.offsetParent || element.parentElement;

        var anchorPosition = {x: anchorElement.totalOffsetLeft, y: anchorElement.totalOffsetTop};

        // FIXME(apavlov@chromium.org): Translate anchorPosition to the element.ownerDocument frame when https://bugs.webkit.org/show_bug.cgi?id=28913 is fixed.
        var anchorBox = {x: anchorPosition.x, y: anchorPosition.y, width: anchorElement.offsetWidth, height: anchorElement.offsetHeight};
        var elementBox = {x: element.totalOffsetLeft, y: element.totalOffsetTop, width: element.offsetWidth, height: element.offsetHeight};
        var newElementPosition = {x: 0, y: 0};

        if (anchorBox.y - elementBox.height >= clippingBox.y)
            newElementPosition.y = anchorBox.y - elementBox.height;
        else
            newElementPosition.y = Math.min(anchorBox.y + anchorBox.height, Math.max(clippingBox.y, clippingBox.y + clippingBox.height - elementBox.height));

        if (anchorBox.x + elementBox.height <= clippingBox.x + clippingBox.height)
            newElementPosition.x = anchorBox.x;
        else
            newElementPosition.x = Math.max(clippingBox.x, clippingBox.x + clippingBox.height - elementBox.height);
        element.positionAt(newElementPosition.x, newElementPosition.y);
    },

    set anchor(x)
    {
        this._checkNotVisible();
        this._anchorElement = x;
    },

    get anchor()
    {
        return this._anchorElement;
    },

    set autoHide(x)
    {
        this._autoHide = x;
    },

    _checkNotVisible: function()
    {
        if (this.visible)
            throw new Error("The popup must not be visible.");
    },

    _keyEventHandler: function(event)
    {
        // Escape hides the popup.
        if (event.keyIdentifier == "U+001B") {
            this.hide();
            event.preventDefault();
            event.handled = true;
        }
    },

    _mouseDownEventHandler: function(event)
    {
        if (this._autoHide && event.originalTarget === this._glasspaneElement)
            this.hide();
    }
}
