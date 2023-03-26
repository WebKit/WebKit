/*
 * Copyright (C) 2016, 2022 Apple Inc. All Rights Reserved.
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

class SliderBase extends LayoutNode
{
    constructor(layoutDelegate, cssClassName = "")
    {
        super(`<div class="slider ${cssClassName}"></div>`);

        // This class should not be instantiated directly. Create a concrete subclass instead.
        console.assert(this.constructor !== SliderBase && this instanceof SliderBase, this);

        this._layoutDelegate = layoutDelegate;

        this.appearanceContainer = new LayoutNode(`<div class="appearance"></div>`);

        this._input = new LayoutNode(`<input type="range" min="0" max="1" step="0.001" />`);
        this._input.element.addEventListener("pointerdown", this);
        this._input.element.addEventListener("input", this);
        this._input.element.addEventListener("change", this);

        this.value = 0;
        this.enabled = true;
        this.isActive = false;
        this._disabled = false;
        this._secondaryValue = 0;

        this._allowsRelativeScrubbing = false;
        this._startValue = NaN;
        this._startPosition = NaN;

        this.children = [this.appearanceContainer, this._input];
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
        // Interrupt any current animation to snap to the new value.
        if (this._animatedValue)
            delete this._animatedValue;

        this._setValueInternal(value);
    }

    setValueAnimated(value)
    {
        if (this.isActive || this._value == value)
            return;

        this._valueAnimation = {
            start: this.value,
            end: value,
            startTime: window.performance.now()
        };

        const duration = 150;

        const animate = currentTime => {
            if (!this._valueAnimation)
                return;

            const elapsedTime = currentTime - this._valueAnimation.startTime;
            const progress = Math.min(elapsedTime / duration, 1);
            const animatedValue = this._valueAnimation.start + (this._valueAnimation.end - this._valueAnimation.start) * progress;

            this._setValueInternal(animatedValue);

            if (progress == 1)
                delete this._valueAnimation;
            else
                requestAnimationFrame(animate);
        };

        requestAnimationFrame(animate);
    }

    _setValueInternal(value)
    {
        if (this.isActive || this._value == value)
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

    get allowsRelativeScrubbing()
    {
        return this._allowsRelativeScrubbing;
    }

    set allowsRelativeScrubbing(allowsRelativeScrubbing)
    {
        this._allowsRelativeScrubbing = !!allowsRelativeScrubbing;

        this.element.classList.toggle("allows-relative-scrubbing", this._allowsRelativeScrubbing);
    }

    // Protected

    handleEvent(event)
    {
        switch (event.type) {
        case "pointerdown":
            this._handlePointerdownEvent(event);
            return;

        case "pointermove":
            this._handlePointermoveEvent(event);
            return;

        case "pointerup":
            this._handlePointerupEvent(event);
            return;

        case "change":
        case "input":
            this._valueDidChange();
            return;
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

    // Private

    _handlePointerdownEvent(event)
    {
        // Interrupt any value animation.
        delete this._valueAnimation;

        this._pointerupTarget = this._interactionEndTarget();
        this._pointerupTarget.addEventListener("pointerup", this, { capture: true });
        if (this._allowsRelativeScrubbing)
            this._pointerupTarget.addEventListener("pointermove", this, { capture: true });

        // We should no longer cache the value since we'll be interacting with the <input>
        // so the value should be read back from it dynamically.
        delete this._value;

        if (this._allowsRelativeScrubbing) {
            this._startValue = parseFloat(this._input.element.value);
            this._startPosition = this._playbackProgress(event.pageX);
        }

        if (this.uiDelegate && typeof this.uiDelegate.controlValueWillStartChanging === "function")
            this.uiDelegate.controlValueWillStartChanging(this);

        this.isActive = true;
        this.appearanceContainer.element.classList.add("changing");

        this.needsLayout = true;
    }

    _interactionEndTarget()
    {
        const mediaControls = this.parentOfType(MediaControls);
        if (mediaControls?.layoutTraits.supportsTouches())
            return mediaControls.element;
        return (!mediaControls || !mediaControls.layoutTraits.isFullscreen) ? window : mediaControls.element;
    }

    _valueDidChange()
    {
        if (this.uiDelegate && typeof this.uiDelegate.controlValueDidChange === "function")
            this.uiDelegate.controlValueDidChange(this);

        this.needsLayout = true;
    }

    _handlePointermoveEvent(event)
    {
        if (!this._allowsRelativeScrubbing || isNaN(this._startValue) || isNaN(this._startPosition))
            return;

        let value = this._startValue + this._playbackProgress(event.pageX) - this._startPosition;
        this._input.element.value = Math.min(Math.max(0, value), 1);
        this._valueDidChange();
    }

    _handlePointerupEvent(event)
    {
        this._pointerupTarget.removeEventListener("pointerup", this, { capture: true });
        this._pointerupTarget.removeEventListener("pointermove", this, { capture: true });
        delete this._pointerupTarget;

        this._startValue = NaN;
        this._startPosition = NaN;

        this.isActive = false;
        this.appearanceContainer.element.classList.remove("changing");

        if (this.uiDelegate && typeof this.uiDelegate.controlValueDidStopChanging === "function")
            this.uiDelegate.controlValueDidStopChanging(this);

        this.needsLayout = true;
    }

    _playbackProgress(pageX)
    {
        let x = window.webkitConvertPointFromPageToNode(this.element, new WebKitPoint(pageX, 0)).x;
        if (this._layoutDelegate?.scaleFactor)
            x *= this._layoutDelegate.scaleFactor;

        return x / this.element.clientWidth;
    }
}
