/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
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
 * @constructor
 */
WebInspector.Dialog = function(relativeToElement, dialogDelegate)
{
    this._dialogDelegate = dialogDelegate;
    this._relativeToElement = relativeToElement;

    // Install glass pane capturing events.
    this._glassPaneElement = document.body.createChild("div");
    this._glassPaneElement.className = "dialog-glass-pane";
    this._glassPaneElement.tabIndex = 0;
    this._glassPaneElement.addEventListener("focus", this._onFocus.bind(this), false);
    
    this._element = this._glassPaneElement.createChild("div");
    this._element.className = "dialog";
    this._element.tabIndex = 0;
    this._element.addEventListener("keydown", this._onKeyDown.bind(this), false);
    this._closeKeys = [
        WebInspector.KeyboardShortcut.Keys.Enter.code,
        WebInspector.KeyboardShortcut.Keys.Esc.code,
    ];

    if (dialogDelegate.okButton)
        dialogDelegate.okButton.addEventListener("click", this._onClick.bind(this), false);
    dialogDelegate.element.addStyleClass("dialog-contents");
    this._element.appendChild(dialogDelegate.element);
    
    this._position();
    this._windowResizeHandler = this._position.bind(this);
    window.addEventListener("resize", this._windowResizeHandler, true);

    this._previousFocusElement = WebInspector.currentFocusElement();
    this._doFocus();
}

WebInspector.Dialog.show = function(relativeToElement, dialogDelegate)
{
    if (WebInspector.Dialog._instance)
        return;
    WebInspector.Dialog._instance = new WebInspector.Dialog(relativeToElement, dialogDelegate);
}

WebInspector.Dialog.prototype = {
    _hide: function()
    {
        if (this._isHiding)
            return;
        this._isHiding = true;

        WebInspector.setCurrentFocusElement(this._previousFocusElement);
        WebInspector.Dialog._instance = null;
        document.body.removeChild(this._glassPaneElement);
        window.removeEventListener("resize", this._windowResizeHandler, true);
    },

    _onFocus: function(event)
    {
        this._hide();
    },

    _doFocus: function()
    {
        if (this._dialogDelegate.defaultFocusedElement) {
            WebInspector.setCurrentFocusElement(this._dialogDelegate.defaultFocusedElement);
            if (typeof(this._dialogDelegate.defaultFocusedElement.select) === "function")
                this._dialogDelegate.defaultFocusedElement.select();
        } else 
            WebInspector.setCurrentFocusElement(this._element);
    },

    _position: function()
    {
        var offset = this._relativeToElement.offsetRelativeToWindow(window);

        var positionX = offset.x + (this._relativeToElement.offsetWidth - this._element.offsetWidth) / 2;
        positionX = Number.constrain(positionX, 0, window.innerWidth - this._element.offsetWidth);
        
        var positionY = offset.y + (this._relativeToElement.offsetHeight - this._element.offsetHeight) / 2;
        positionY = Number.constrain(positionY, 0, window.innerHeight - this._element.offsetHeight);
        
        this._element.style.left = positionX + "px";
        this._element.style.top = positionY + "px";
    },

    _onKeyDown: function(event)
    {
        if (event.keyCode === WebInspector.KeyboardShortcut.Keys.Tab.code) {
            event.preventDefault();
            return;
        }

        if (event.keyCode === WebInspector.KeyboardShortcut.Keys.Enter.code)
            this._dialogDelegate.onAction();

        if (this._closeKeys.indexOf(event.keyCode) >= 0) {
            this._hide();
            event.preventDefault();
            event.stopPropagation();
        }
    },

    _onClick: function(event)
    {
        this._dialogDelegate.onAction();
        this._hide();
    }
};

/**
 * @interface
 */
WebInspector.DialogDelegate = function()
{
}

WebInspector.DialogDelegate.prototype = {
    get defaultFocusedElement() { },
    
    get okButton() { },
    
    onAction: function() { }
};
