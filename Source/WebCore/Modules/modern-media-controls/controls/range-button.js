/*
 * Copyright (C) 2021 Apple Inc. All Rights Reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

class RangeButton extends Button
{

    constructor({ layoutDelegate = null, cssClassName = "", iconName = "" } = {})
    {
        super({ layoutDelegate, cssClassName, iconName });

        this._value = 0;

        this.element.classList.add("range");
        this.element.addEventListener("pointerdown", this);

        this._indicator = this.addChild(new LayoutNode(`<div class="indicator"></div>`), 0);
        this._indicatorTint = this._indicator.addChild(new BackgroundTint);
        this._indicatorFill = this._indicator.addChild(new LayoutNode(`<div class="fill"></div>`));

        this._indicator.visible = false;

        if (GestureRecognizer.SupportsTouches)
            this._tapGestureRecognizer.enabled = false;
        else
            this.element.removeEventListener("click", this);
    }

    // Public

    get value()
    {
        return this._value;
    }

    set value(value)
    {
        this._value = value;
        this.markDirtyProperty("value");
    }

    // Protected

    commitProperty(propertyName)
    {
        if (propertyName === "value")
            this._indicatorFill.element.style.height = `${this._value * 100}%`;
        else
            super.commitProperty(propertyName);
    }

    handleEvent(event)
    {
        if (event.currentTarget === this.element && event.type === "pointerdown")
            this._handlePointerdown(event);
        else if (event.currentTarget === this._pointerMoveAndEndTarget() && event.type === "pointermove")
            this._handlePointermove(event);
        else if (event.currentTarget === this._pointerMoveAndEndTarget() && event.type === "pointerup")
            this._handlePointerup(event);
        else
            super.handleEvent(event);
    }

    // Private

    _pointerMoveAndEndTarget()
    {
        const mediaControls = this.parentOfType(MediaControls);
        return (!mediaControls || !mediaControls.layoutTraits.isFullscreen) ? window : mediaControls.element;
    }

    _handlePointerdown(event)
    {
        this._pointerMoveAndEndTarget().addEventListener("pointermove", this, true);
        this._pointerMoveAndEndTarget().addEventListener("pointerup", this, true);

        this._initialPointerY = event.clientY;
        this._initialValue = this._value;
    }

    _handlePointermove(event)
    {
        if (!this._indicator.visible) {
            this._indicator.visible = true;
            if (this.uiDelegate && typeof this.uiDelegate.controlValueWillStartChanging === "function")
                this.uiDelegate.controlValueWillStartChanging(this);
            return;
        }

        if (!this._indicatorHeight)
            this._indicatorHeight = this._indicator.computedValueForStylePropertyInPx("height");

        if (!this._indicatorHeight)
            return;

        const traveledPointerDistance = this._initialPointerY - event.clientY;
        const valueIncrement = traveledPointerDistance / this._indicatorHeight;

        const newValue = Math.max(0, Math.min(1, this._initialValue + valueIncrement));
        if (this._value === newValue)
            return;

        this.value = newValue;

        if (this.uiDelegate && typeof this.uiDelegate.controlValueDidChange === "function")
            this.uiDelegate.controlValueDidChange(this);
    }

    _handlePointerup(event)
    {
        this._pointerMoveAndEndTarget().removeEventListener("pointermove", this, true);
        this._pointerMoveAndEndTarget().removeEventListener("pointerup", this, true);

        delete this._initialPointerY;
        delete this._initialValue;

        if (this._indicator.visible) {
            if (this.uiDelegate && typeof this.uiDelegate.controlValueDidStopChanging === "function")
                this.uiDelegate.controlValueDidStopChanging(this);
            this._indicator.visible = false;
        } else {
            if (this._enabled && this.uiDelegate && typeof this.uiDelegate.buttonWasPressed === "function")
                this.uiDelegate.buttonWasPressed(this);
        }
    }

}
