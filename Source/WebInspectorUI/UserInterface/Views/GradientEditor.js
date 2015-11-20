/*
 * Copyright (C) 2014 Apple Inc. All rights reserved.
 * Copyright (C) 2015 Devin Rousso <dcrousso+webkit@gmail.com>. All rights reserved.
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

WebInspector.GradientEditor = class GradientEditor extends WebInspector.Object
{
    constructor()
    {
        super();

        this._element = document.createElement("div");
        this._element.classList.add("gradient-editor");

        this._gradient = null;
        this._gradientTypes = {
            "linear-gradient": {
                type: WebInspector.LinearGradient,
                label: WebInspector.UIString("Linear Gradient"),
                repeats: false
            },
            "radial-gradient": {
                type: WebInspector.RadialGradient,
                label: WebInspector.UIString("Radial Gradient"),
                repeats: false
            },
            "repeating-linear-gradient": {
                type: WebInspector.LinearGradient,
                label: WebInspector.UIString("Repeating Linear Gradient"),
                repeats: true
            },
            "repeating-radial-gradient": {
                type: WebInspector.RadialGradient,
                label: WebInspector.UIString("Repeating Radial Gradient"),
                repeats: true
            }
        };
        this._editingColor = false;

        this._gradientTypePicker = this._element.appendChild(document.createElement("select"));
        this._gradientTypePicker.classList.add("gradient-type-select");
        for (let type in this._gradientTypes) {
            let option = this._gradientTypePicker.appendChild(document.createElement("option"));
            option.value = type;
            option.text = this._gradientTypes[type].label;
        }
        this._gradientTypePicker.addEventListener("change", this._gradientTypeChanged.bind(this));

        this._gradientSlider = new WebInspector.GradientSlider(this);
        this._element.appendChild(this._gradientSlider.element);

        this._colorPicker = new WebInspector.ColorPicker;
        this._colorPicker.colorWheel.dimension = 190;
        this._colorPicker.addEventListener(WebInspector.ColorPicker.Event.ColorChanged, this._colorPickerColorChanged, this);

        let angleLabel = this._element.appendChild(document.createElement("label"));
        angleLabel.classList.add("gradient-angle");
        angleLabel.append(WebInspector.UIString("Angle"));

        this._angleInput = angleLabel.appendChild(document.createElement("input"));
        this._angleInput.type = "text";
        this._angleInput.addEventListener("input", this._angleChanged.bind(this));

        let dragToAdjustController = new WebInspector.DragToAdjustController(this);
        dragToAdjustController.element = angleLabel;
        dragToAdjustController.enabled = true;
    }

    get element()
    {
        return this._element;
    }

    set gradient(gradient)
    {
        if (!gradient)
            return;

        const isLinear = gradient instanceof WebInspector.LinearGradient;
        const isRadial = gradient instanceof WebInspector.RadialGradient;
        console.assert(isLinear || isRadial);
        if (!isLinear && !isRadial)
            return;

        this._gradient = gradient;
        this._gradientSlider.stops = this._gradient.stops;
        if (isLinear) {
            this._gradientTypePicker.value = this._gradient.repeats ? "repeating-linear-gradient" : "linear-gradient";
            this._angleInput.value = this._gradient.angle + "\u00B0";
        } else
            this._gradientTypePicker.value = this._gradient.repeats ? "repeating-radial-gradient" : "radial-gradient";

        this._updateCSSClassForGradientType();
    }

    get gradient()
    {
        return this._gradient;
    }

    // Protected

    gradientSliderStopsDidChange(gradientSlider)
    {
        this._gradient.stops = gradientSlider.stops;

        this.dispatchEventToListeners(WebInspector.GradientEditor.Event.GradientChanged, {gradient: this._gradient});
    }

    gradientSliderStopWasSelected(gradientSlider, stop)
    {
        const selectedStop = gradientSlider.selectedStop;
        if (selectedStop && !this._editingColor) {
            this._element.appendChild(this._colorPicker.element);
            this._element.classList.add("editing-color");
            this._colorPicker.color = selectedStop.color;
            this._editingColor = true;
        } else if (!selectedStop) {
            this._colorPicker.element.remove();
            this._element.classList.remove("editing-color");
            this._editingColor = false;
        }

        // Ensure the angle input is not focused since, if it were, it'd make a scrollbar appear as we
        // animate the popover's frame to fit its new content.
        this._angleInput.blur();

        this.dispatchEventToListeners(WebInspector.GradientEditor.Event.ColorPickerToggled);
        this.dispatchEventToListeners(WebInspector.GradientEditor.Event.GradientChanged, {gradient: this._gradient});
    }

    dragToAdjustControllerWasAdjustedByAmount(dragToAdjustController, amount)
    {
        const angleInputValue = parseFloat(this._angleInput.value);
        if (isNaN(angleInputValue))
            return;

        let angle = angleInputValue + amount;
        if (Math.round(angle) !== angle)
            angle = angle.toFixed(1);

        this._angleInput.value = angle;
        this._angleInputValueDidChange(angle);
    }

    // Private

    _updateCSSClassForGradientType()
    {
        const isRadial = this._gradient instanceof WebInspector.RadialGradient;
        this._element.classList.toggle("radial-gradient", isRadial);

        this.dispatchEventToListeners(WebInspector.GradientEditor.Event.ColorPickerToggled);
    }

    _gradientTypeChanged(event)
    {
        const descriptor = this._gradientTypes[this._gradientTypePicker.value];
        if (!(this._gradient instanceof descriptor.type)) {
            if (descriptor.type === WebInspector.LinearGradient) {
                this._gradient = new WebInspector.LinearGradient(180, this._gradient.stops);
                this._angleInput.value = "180\u00B0";
            } else
                this._gradient = new WebInspector.RadialGradient("", this._gradient.stops);

            this._updateCSSClassForGradientType();
        }
        this._gradient.repeats = descriptor.repeats;

        this.dispatchEventToListeners(WebInspector.GradientEditor.Event.GradientChanged, {gradient: this._gradient});
    }

    _colorPickerColorChanged(event)
    {
        this._gradientSlider.selectedStop.color = event.target.color;
        this._gradientSlider.stops = this._gradient.stops;

        this.dispatchEventToListeners(WebInspector.GradientEditor.Event.GradientChanged, {gradient: this._gradient});
    }

    _angleChanged(event)
    {
        const angle = parseFloat(this._angleInput.value) || 0;
        if (isNaN(angle))
            return;

        this._angleInputValueDidChange(angle);
    }

    _angleInputValueDidChange(angle)
    {
        this._gradient.angle = angle;
        const matches = this._angleInput.value.match(/\u00B0/g);
        if (!matches || matches.length !== 1) {
            const savedStart = this._angleInput.selectionStart;
            this._angleInput.value = angle + "\u00B0";
            this._angleInput.selectionStart = savedStart;
            this._angleInput.selectionEnd = savedStart;
        }

        this.dispatchEventToListeners(WebInspector.GradientEditor.Event.GradientChanged, {gradient: this._gradient});
    }
}

WebInspector.GradientEditor.Event = {
    GradientChanged: "gradient-editor-gradient-changed",
    ColorPickerToggled: "gradient-editor-color-picker-toggled"
};
