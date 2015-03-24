/*
 * Copyright (C) 2015 University of Washington.
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

WebInspector.Resizer = function(ruleOrientation, delegate) {
    // FIXME: Convert this to a WebInspector.Object subclass, and call super().
    // WebInspector.Object.call(this);

    console.assert(delegate);

    this._delegate = delegate;
    this._orientation = ruleOrientation;
    this._element = document.createElement("div");
    this._element.classList.add(WebInspector.Resizer.StyleClassName);

    if (this._orientation === WebInspector.Resizer.RuleOrientation.Horizontal)
        this._element.classList.add(WebInspector.Resizer.HorizontalRuleStyleClassName);
    else if (this._orientation === WebInspector.Resizer.RuleOrientation.Vertical)
        this._element.classList.add(WebInspector.Resizer.VerticalRuleStyleClassName);

    this._element.addEventListener("mousedown", this._resizerMouseDown.bind(this), false);
    this._resizerMouseMovedEventListener = this._resizerMouseMoved.bind(this);
    this._resizerMouseUpEventListener = this._resizerMouseUp.bind(this);
};

WebInspector.Resizer.RuleOrientation = {
    Horizontal: Symbol("resizer-rule-orientation-horizontal"),
    Vertical: Symbol("resizer-rule-orientation-vertical"),
};

WebInspector.Resizer.StyleClassName = "resizer";
WebInspector.Resizer.HorizontalRuleStyleClassName = "horizontal-rule";
WebInspector.Resizer.VerticalRuleStyleClassName = "vertical-rule";
WebInspector.Resizer.GlassPaneStyleClassName = "glass-pane-for-drag";

WebInspector.Resizer.prototype = {
    constructor: WebInspector.Resizer,
    __proto__: WebInspector.Object.prototype,

    // Public

    get element()
    {
        return this._element;
    },

    get orientation()
    {
        return this._orientation;
    },

    get initialPosition()
    {
        return this._resizerMouseDownPosition || NaN;
    },

    // Private

    _currentPosition: function()
    {
        if (this._orientation === WebInspector.Resizer.RuleOrientation.Vertical)
            return event.pageX;
        if (this._orientation === WebInspector.Resizer.RuleOrientation.Horizontal)
            return event.pageY;

        console.assert(false, "Should not be reached!");
    },

    _resizerMouseDown: function(event)
    {
        if (event.button !== 0 || event.ctrlKey)
            return;

        this._resizerMouseDownPosition = this._currentPosition();

        var delegateRequestedAbort = false;
        if (typeof this._delegate.resizerDragStarted === "function")
            delegateRequestedAbort = this._delegate.resizerDragStarted(this, event.target);

        if (delegateRequestedAbort) {
            delete this._resizerMouseDownPosition;
            return;
        }

        if (this._orientation === WebInspector.Resizer.RuleOrientation.Vertical)
            document.body.style.cursor = "col-resize";
        else {
            console.assert(this._orientation === WebInspector.Resizer.RuleOrientation.Horizontal);
            document.body.style.cursor = "row-resize";
        }

        // Register these listeners on the document so we can track the mouse if it leaves the resizer.
        document.addEventListener("mousemove", this._resizerMouseMovedEventListener, false);
        document.addEventListener("mouseup", this._resizerMouseUpEventListener, false);

        event.preventDefault();
        event.stopPropagation();

        // Install a global "glass pane" which prevents cursor from changing during the drag interaction.
        // The cursor could change when hovering over links, text, or other elements with cursor cues.
        // FIXME: when Pointer Events support is available this could be implemented by drawing the cursor ourselves.
        if (WebInspector._elementDraggingGlassPane)
            WebInspector._elementDraggingGlassPane.parentElement.removeChild(WebInspector._elementDraggingGlassPane);

        var glassPaneElement = document.createElement("div");
        glassPaneElement.className = WebInspector.Resizer.GlassPaneStyleClassName;
        document.body.appendChild(glassPaneElement);
        WebInspector._elementDraggingGlassPane = glassPaneElement;
    },

    _resizerMouseMoved: function(event)
    {
        event.preventDefault();
        event.stopPropagation();

        if (typeof this._delegate.resizerDragging === "function")
            this._delegate.resizerDragging(this, this._resizerMouseDownPosition - this._currentPosition());
    },

    _resizerMouseUp: function(event)
    {
        if (event.button !== 0 || event.ctrlKey)
            return;

        document.body.style.removeProperty("cursor");

        if (WebInspector._elementDraggingGlassPane) {
            WebInspector._elementDraggingGlassPane.parentElement.removeChild(WebInspector._elementDraggingGlassPane);
            delete WebInspector._elementDraggingGlassPane;
        }

        document.removeEventListener("mousemove", this._resizerMouseMovedEventListener, false);
        document.removeEventListener("mouseup", this._resizerMouseUpEventListener, false);

        event.preventDefault();
        event.stopPropagation();

        if (typeof this._delegate.resizerDragEnded === "function")
            this._delegate.resizerDragEnded(this);

        delete this._resizerMouseDownPosition;
    }
};
