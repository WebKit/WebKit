/*
 * Copyright (C) 2014 Antoine Quint
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

WebInspector.DragToAdjustController = function(delegate)
{
    this._delegate = delegate;

    this._element = null;
    this._active = false;
    this._enabled = false;
    this._dragging = false;
    this._tracksMouseClickAndDrag = false;
};

WebInspector.DragToAdjustController.StyleClassName = "drag-to-adjust";

WebInspector.DragToAdjustController.prototype = {
    constructor: WebInspector.DragToAdjustController,

    // Public

    get element()
    {
        return this._element;
    },

    set element(element)
    {
        this._element = element;
    },

    set enabled(enabled)
    {
        if (this._enabled === enabled)
            return;

        if (enabled) {
            this._element.addEventListener("mouseenter", this);
            this._element.addEventListener("mouseleave", this);
        } else {
            this._element.removeEventListener("mouseenter", this);
            this._element.removeEventListener("mouseleave", this);
        }
    },

    get active()
    {
        return this._active;
    },

    set active(active)
    {
        if (!this._element)
            return;
        
        if (this._active === active)
            return;

        if (active) {
            WebInspector.notifications.addEventListener(WebInspector.Notification.GlobalModifierKeysDidChange, this._modifiersDidChange, this);
            this._element.addEventListener("mousemove", this);
        } else {
            WebInspector.notifications.removeEventListener(WebInspector.Notification.GlobalModifierKeysDidChange, this._modifiersDidChange, this);
            this._element.removeEventListener("mousemove", this);
            this._setTracksMouseClickAndDrag(false);
        }

        this._active = active;

        if (this._delegate && typeof this._delegate.dragToAdjustControllerActiveStateChanged === "function")
            this._delegate.dragToAdjustControllerActiveStateChanged(this);
    },

    reset: function()
    {
        this._setTracksMouseClickAndDrag(false);
        this._element.classList.remove(WebInspector.DragToAdjustController.StyleClassName);

        if (this._delegate && typeof this._delegate.dragToAdjustControllerDidReset === "function")
            this._delegate.dragToAdjustControllerDidReset(this);
    },

    // Protected

    handleEvent: function(event)
    {
        switch(event.type) {
        case "mouseenter":
            if (!this._dragging) {
                if (this._delegate && typeof this._delegate.dragToAdjustControllerCanBeActivated === "function")
                    this.active = this._delegate.dragToAdjustControllerCanBeActivated(this);
                else
                    this.active = true;
            }
            break;
        case "mouseleave":
            if (!this._dragging)
                this.active = false;
            break;
        case "mousemove":
            if (this._dragging)
                this._mouseWasDragged(event);
            else
                this._mouseMoved(event);
            break;
        case "mousedown":
            this._mouseWasPressed(event);
            break;
        case "mouseup":
            this._mouseWasReleased(event);
            break;
        case "contextmenu":
            event.preventDefault();
            break;
        }
    },

    // Private

    _setDragging: function(dragging)
    {
        if (this._dragging === dragging)
            return;
        
        console.assert(window.event);
        if (dragging)
            WebInspector.elementDragStart(this._element, this, this, window.event, "col-resize", window);
        else
            WebInspector.elementDragEnd(window.event);

        this._dragging = dragging;
    },

    _setTracksMouseClickAndDrag: function(tracksMouseClickAndDrag)
    {
        if (this._tracksMouseClickAndDrag === tracksMouseClickAndDrag)
            return;
        
        if (tracksMouseClickAndDrag) {
            this._element.classList.add(WebInspector.DragToAdjustController.StyleClassName);
            window.addEventListener("mousedown", this, true);
            window.addEventListener("contextmenu", this, true);
        } else {
            this._element.classList.remove(WebInspector.DragToAdjustController.StyleClassName);
            window.removeEventListener("mousedown", this, true);
            window.removeEventListener("contextmenu", this, true);
            this._setDragging(false);
        }
        
        this._tracksMouseClickAndDrag = tracksMouseClickAndDrag;
    },

    _modifiersDidChange: function(event)
    {
        var canBeAdjusted = WebInspector.modifierKeys.altKey;
        if (canBeAdjusted && this._delegate && typeof this._delegate.dragToAdjustControllerCanBeAdjusted === "function")
            canBeAdjusted = this._delegate.dragToAdjustControllerCanBeAdjusted(this);

        this._setTracksMouseClickAndDrag(canBeAdjusted);
    },
    
    _mouseMoved: function(event)
    {
        var canBeAdjusted = event.altKey;
        if (canBeAdjusted && this._delegate && typeof this._delegate.dragToAdjustControllerCanAdjustObjectAtPoint === "function")
            canBeAdjusted = this._delegate.dragToAdjustControllerCanAdjustObjectAtPoint(this, WebInspector.Point.fromEvent(event));

        this._setTracksMouseClickAndDrag(canBeAdjusted);
    },
    
    _mouseWasPressed: function(event)
    {
        this._lastX = event.screenX;

        this._setDragging(true);

        event.preventDefault();
        event.stopPropagation();
    },

    _mouseWasDragged: function(event)
    {
        var x = event.screenX;
        var amount = x - this._lastX;

        if (Math.abs(amount) < 1)
            return;

        this._lastX = x;

        if (event.ctrlKey)
            amount /= 10;
        else if (event.shiftKey)
            amount *= 10;

        if (this._delegate && typeof this._delegate.dragToAdjustControllerWasAdjustedByAmount === "function")
            this._delegate.dragToAdjustControllerWasAdjustedByAmount(this, amount);

        event.preventDefault();
        event.stopPropagation();
    },

    _mouseWasReleased: function(event)
    {
        this._setDragging(false);

        event.preventDefault();
        event.stopPropagation();

        this.reset();
    }
};
