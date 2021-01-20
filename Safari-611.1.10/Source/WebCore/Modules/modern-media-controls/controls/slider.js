/*
 * Copyright (C) 2016 Apple Inc. All Rights Reserved.
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

class Slider extends LayoutNode
{

    constructor(cssClassName = "")
    {
        super(`<div class="slider ${cssClassName}"></div>`);

        this._container = new LayoutNode(`<div class="custom-slider"></div>`);
        this._track = new LayoutNode(`<div class="track fill"></div>`);
        this._primaryFill = new LayoutNode(`<div class="primary fill"></div>`);
        this._secondaryFill = new LayoutNode(`<div class="secondary fill"></div>`);
        this._knob = new LayoutNode(`<div class="knob"></div>`);
        this._container.children = [this._track, this._primaryFill, this._secondaryFill, this._knob];

        this._input = new LayoutNode(`<input type="range" min="0" max="1" step="0.001" />`);
        this._input.element.addEventListener("pointerdown", this);
        this._input.element.addEventListener("input", this);
        this._input.element.addEventListener("change", this);

        this.value = 0;
        this.height = 16;
        this.enabled = true;
        this.isActive = false;
        this._secondaryValue = 0;
        this._disabled = false;

        this.children = [this._container, this._input];
    }

    // Public

    set inputAccessibleLabel(timeValue)
    {
        this._input.element.setAttribute("aria-valuetext", formattedStringForDuration(timeValue));
    }

    get disabled()
    {
        return this._disabled;
    }

    set disabled(flag)
    {
        if (this._disabled === flag)
            return;

        this._disabled = flag;
        this.markDirtyProperty("disabled");
    }

    get value()
    {
        if (this._value !== undefined)
            return this._value;
        return parseFloat(this._input.element.value);
    }

    set value(value)
    {
        if (this.isActive)
            return;

        this._value = value;
        this.markDirtyProperty("value");
        this.needsLayout = true;
    }

    get secondaryValue()
    {
        return this._secondaryValue;
    }

    set secondaryValue(secondaryValue)
    {
        if (this._secondaryValue === secondaryValue)
            return;

        this._secondaryValue = secondaryValue;
        this.needsLayout = true;
    }

    // Protected

    handleEvent(event)
    {
        switch (event.type) {
        case "pointerdown":
            this._handlePointerdownEvent();
            break;
        case "pointerup":
            this._handlePointerupEvent();
            break;
        case "change":
        case "input":
            this._valueDidChange();
            break;
        }
    }

    commitProperty(propertyName)
    {
        switch (propertyName) {
        case "value":
            this._input.element.value = this._value;
            delete this._value;
            break;
        case "disabled":
            this.element.classList.toggle("disabled", this._disabled);
            break;
        default :
            super.commitProperty(propertyName);
            break;
        }
    }

    commit()
    {
        super.commit();

        const scrubberRadius = 4.5;
        const scrubberCenterX = scrubberRadius + Math.round((this.width - (scrubberRadius * 2)) * this.value);
        this._primaryFill.element.style.width = `${scrubberCenterX}px`;
        this._secondaryFill.element.style.left = `${scrubberCenterX}px`;
        this._secondaryFill.element.style.right = `${(1 - this._secondaryValue) * 100}%`;
        this._knob.element.style.left = `${scrubberCenterX}px`;
    }

    // Private

    _handlePointerdownEvent()
    {
        this._pointerupTarget = this._interactionEndTarget();
        this._pointerupTarget.addEventListener("pointerup", this, true);

        this._valueWillStartChanging();
    }

    _interactionEndTarget()
    {
        const mediaControls = this.parentOfType(MediaControls);
        return (!mediaControls || mediaControls instanceof MacOSInlineMediaControls) ? window : mediaControls.element;
    }

    _valueWillStartChanging()
    {
        // We should no longer cache the value since we'll be interacting with the <input>
        // so the value should be read back from it dynamically.
        delete this._value;

        if (this.uiDelegate && typeof this.uiDelegate.controlValueWillStartChanging === "function")
            this.uiDelegate.controlValueWillStartChanging(this);
        this.isActive = true;
        this.needsLayout = true;
    }

    _valueDidChange()
    {
        if (this.uiDelegate && typeof this.uiDelegate.controlValueDidChange === "function")
            this.uiDelegate.controlValueDidChange(this);

        this.needsLayout = true;
    }

    _valueDidStopChanging()
    {
        this.isActive = false;
        if (this.uiDelegate && typeof this.uiDelegate.controlValueDidStopChanging === "function")
            this.uiDelegate.controlValueDidStopChanging(this);

        this.needsLayout = true;
    }

    _handlePointerupEvent()
    {
        this._pointerupTarget.removeEventListener("pointerup", this, true);
        delete this._pointerupTarget;

        this._valueDidStopChanging();
    }
}
