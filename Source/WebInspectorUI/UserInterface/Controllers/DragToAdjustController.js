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

WI.DragToAdjustController = class DragToAdjustController
{
    constructor(delegate)
    {
        this._delegate = delegate;

        this._element = null;
        this._active = false;
        this._enabled = false;
        this._dragging = false;
        this._tracksMouseClickAndDrag = false;

        this._boundHandleMouseEnter = null;
        this._boundHandleMouseLeave = null;
        this._boundHandleMouseMove = null;
        this._boundMouseWasPressed = null;
    }

    // Public

    get element()
    {
        return this._element;
    }

    set element(element)
    {
        this._element = element;
    }

    get enabled()
    {
        return this._enabled;
    }

    set enabled(enabled)
    {
        if (this._enabled === enabled)
            return;

        if (enabled) {
            this._boundHandleMouseEnter ||= this._handleMouseEnter.bindWeak(this);
            this._element.addEventListener("mouseenter", this._boundHandleMouseEnter);

            this._boundHandleMouseLeave ||= this._handleMouseLeave.bindWeak(this);
            this._element.addEventListener("mouseleave", this._boundHandleMouseLeave);
        } else {
            this._element.removeEventListener("mouseenter", this._boundHandleMouseEnter);
            this._element.removeEventListener("mouseleave", this._boundHandleMouseLeave);
        }
    }

    get active()
    {
        return this._active;
    }

    set active(active)
    {
        if (!this._element)
            return;

        if (this._active === active)
            return;

        if (active) {
            WI.notifications.addEventListener(WI.Notification.GlobalModifierKeysDidChange, this._modifiersDidChange, this);

            this._boundHandleMouseMove ||= this._handleMouseMove.bindWeak(this);
            this._element.addEventListener("mousemove", this._boundHandleMouseMove);
        } else {
            WI.notifications.removeEventListener(WI.Notification.GlobalModifierKeysDidChange, this._modifiersDidChange, this);
            this._element.removeEventListener("mousemove", this._boundHandleMouseMove);
            this._setTracksMouseClickAndDrag(false);
        }

        this._active = active;

        if (this._delegate && typeof this._delegate.dragToAdjustControllerActiveStateChanged === "function")
            this._delegate.dragToAdjustControllerActiveStateChanged(this);
    }

    reset()
    {
        this._setTracksMouseClickAndDrag(false);
        this._element.classList.remove(WI.DragToAdjustController.StyleClassName);

        if (this._delegate && typeof this._delegate.dragToAdjustControllerDidReset === "function")
            this._delegate.dragToAdjustControllerDidReset(this);
    }

    // Private

    _setDragging(dragging)
    {
        if (this._dragging === dragging)
            return;

        console.assert(window.event);
        if (dragging)
            WI.elementDragStart(this._element, this, this, window.event, "col-resize", window);
        else
            WI.elementDragEnd(window.event);

        this._dragging = dragging;
    }

    _setTracksMouseClickAndDrag(tracksMouseClickAndDrag)
    {
        if (this._tracksMouseClickAndDrag === tracksMouseClickAndDrag)
            return;

        if (tracksMouseClickAndDrag) {
            this._element.classList.add(WI.DragToAdjustController.StyleClassName);

            this._boundMouseWasPressed ||= this._mouseWasPressed.bindWeak(this);
            window.addEventListener("mousedown", this._boundMouseWasPressed, {capture: true});

            window.addEventListener("contextmenu", this._handleContextMenu, {capture: true});
        } else {
            this._element.classList.remove(WI.DragToAdjustController.StyleClassName);
            window.removeEventListener("mousedown", this._boundMouseWasPressed, {capture: true});
            window.removeEventListener("contextmenu", this._handleContextMenu, {capture: true});
            this._setDragging(false);
        }

        this._tracksMouseClickAndDrag = tracksMouseClickAndDrag;
    }

    _modifiersDidChange(event)
    {
        var canBeAdjusted = WI.modifierKeys.altKey;
        if (canBeAdjusted && this._delegate && typeof this._delegate.dragToAdjustControllerCanBeAdjusted === "function")
            canBeAdjusted = this._delegate.dragToAdjustControllerCanBeAdjusted(this);

        this._setTracksMouseClickAndDrag(canBeAdjusted);
    }

    _mouseMoved(event)
    {
        var canBeAdjusted = event.altKey;
        if (canBeAdjusted && this._delegate && typeof this._delegate.dragToAdjustControllerCanAdjustObjectAtPoint === "function")
            canBeAdjusted = this._delegate.dragToAdjustControllerCanAdjustObjectAtPoint(this, WI.Point.fromEvent(event));

        this._setTracksMouseClickAndDrag(canBeAdjusted);
    }

    _mouseWasPressed(event)
    {
        this._lastX = event.screenX;

        this._setDragging(true);

        event.preventDefault();
        event.stopPropagation();
    }

    _mouseWasDragged(event)
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
    }

    _handleMouseEnter(event)
    {
        if (!this._dragging) {
            if (this._delegate && typeof this._delegate.dragToAdjustControllerCanBeActivated === "function")
                this.active = this._delegate.dragToAdjustControllerCanBeActivated(this);
            else
                this.active = true;
        }
    }

    _handleMouseLeave(event)
    {
        if (!this._dragging)
            this.active = false;
    }

    _handleMouseMove(event)
    {
        if (this._dragging)
            this._mouseWasDragged(event);
        else
            this._mouseMoved(event);
    }

    _handleContextMenu(event)
    {
        event.preventDefault();
    }
};

WI.DragToAdjustController.StyleClassName = "drag-to-adjust";
