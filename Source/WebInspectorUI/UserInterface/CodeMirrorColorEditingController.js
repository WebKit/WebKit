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

WebInspector.CodeMirrorColorEditingController = function(codeMirror, marker)
{
    WebInspector.Object.call(this);

    this._codeMirror = codeMirror;
    this._marker = marker;
    this._delegate = null;

    this._range = marker.range;
    this._color = WebInspector.Color.fromString(this.text);

    this._keyboardShortcutEsc = new WebInspector.KeyboardShortcut(null, WebInspector.KeyboardShortcut.Key.Escape);
}

WebInspector.CodeMirrorColorEditingController.prototype = {
    constructor: WebInspector.CodeMirrorColorEditingController,
    __proto__: WebInspector.Object.prototype,
    
    // Public
    
    get marker()
    {
        return this._marker;
    },
    
    get range()
    {
        return this._range;
    },

    get color()
    {
        return this._color;
    },
    
    set color(color)
    {
        this.text = color.toString();
        this._color = color;
    },

    get delegate()
    {
        return this._delegate;
    },

    set delegate(delegate)
    {
        this._delegate = delegate;
    },

    get text()
    {
        var from = {line: this._range.startLine, ch: this._range.startColumn};
        var to = {line: this._range.endLine, ch: this._range.endColumn};
        return this._codeMirror.getRange(from, to);
    },
    
    set text(text)
    {
        var from = {line: this._range.startLine, ch: this._range.startColumn};
        var to = {line: this._range.endLine, ch: this._range.endColumn};
        this._codeMirror.replaceRange(text, from, to);

        var lines = text.split("\n");
        var endLine = this._range.startLine + lines.length - 1;
        var endColumn = lines.length > 1 ? lines.lastValue.length : this._range.startColumn + text.length;
        this._range = new WebInspector.TextRange(this._range.startLine, this._range.startColumn, endLine, endColumn);
    },
    
    presentHoverMenu: function()
    {
        this._hoverMenu = new WebInspector.HoverMenu(this);
        this._hoverMenu.element.classList.add("color");
        this._bounds = this._marker.bounds;
        this._hoverMenu.present(this._bounds);
    },

    dismissHoverMenu: function()
    {
        this._hoverMenu.dismiss();
    },

    // Protected

    handleEvent: function(event)
    {
        if (!this._keyboardShortcutEsc.matchesEvent(event) || !this._popover.visible)
            return;
        
        this.color = this._originalColor;
        this._popover.dismiss();

        event.stopPropagation();
        event.preventDefault();
    },

    hoverMenuButtonWasPressed: function(hoverMenu)
    {
        var colorPicker = new WebInspector.ColorPicker;
        colorPicker.addEventListener(WebInspector.ColorPicker.Event.ColorChanged, this._colorPickerColorChanged, this);

        this._popover = new WebInspector.Popover(this);
        this._popover.content = colorPicker.element;
        this._popover.present(this._bounds.pad(2), [WebInspector.RectEdge.MAX_Y, WebInspector.RectEdge.MAX_X]);

        window.addEventListener("keydown", this, true);

        colorPicker.color = this._color;

        hoverMenu.dismiss();

        if (this._delegate && typeof this._delegate.colorEditingControllerDidStartEditing === "function")
            this._delegate.colorEditingControllerDidStartEditing(this);

        this._originalColor = this._color.copy();
    },
    
    didDismissPopover: function(popover)
    {
        delete this._popover;
        delete this._originalColor;

        window.removeEventListener("keydown", this, true);

        if (this._delegate && typeof this._delegate.colorEditingControllerDidFinishEditing === "function")
            this._delegate.colorEditingControllerDidFinishEditing(this);
    },
    
    // Private
    
    _colorPickerColorChanged: function(event)
    {
        this.color = event.target.color;
    }    
}
