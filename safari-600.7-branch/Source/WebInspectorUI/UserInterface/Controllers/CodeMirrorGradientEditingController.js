/*
 * Copyright (C) 2014 Apple Inc. All rights reserved.
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

WebInspector.CodeMirrorGradientEditingController = function(codeMirror, marker)
{
    WebInspector.CodeMirrorEditingController.call(this, codeMirror, marker);

    if (!WebInspector.CodeMirrorGradientEditingController.GradientTypes) {
        WebInspector.CodeMirrorGradientEditingController.GradientTypes = {
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
    }
}

WebInspector.CodeMirrorGradientEditingController.StyleClassName = "gradient-editing-controller";
WebInspector.CodeMirrorGradientEditingController.EditsColorClassName = "edits-color";
WebInspector.CodeMirrorGradientEditingController.RadialGradientClassName = "radial-gradient";

// Lazily populated in the WebInspector.CodeMirrorGradientEditingController constructor.
// It needs to be lazy to use UIString after Main.js and localizedStrings.js loads.
WebInspector.CodeMirrorGradientEditingController.GradientTypes = null;

WebInspector.CodeMirrorGradientEditingController.prototype = {
    constructor: WebInspector.CodeMirrorGradientEditingController,
    __proto__: WebInspector.CodeMirrorEditingController.prototype,

    // Public

    get initialValue()
    {
        return WebInspector.Gradient.fromString(this.text);
    },

    get cssClassName()
    {
        return "gradient";
    },

    get popoverPreferredEdges()
    {
        // Since the gradient editor can resize to be quite tall, let's avoid displaying the popover
        // above the edited value so that it may not change which edge it attaches to upon editing a stop.
        return [WebInspector.RectEdge.MIN_X, WebInspector.RectEdge.MAX_Y, WebInspector.RectEdge.MAX_X];
    },

    popoverTargetFrameWithRects: function(rects)
    {
        // If a gradient is defined across several lines, we probably want to use the first line only
        // as a target frame for the editor since we may reformat the gradient value to fit on a single line.
        return rects[0];
    },

    popoverWillPresent: function(popover)
    {
        this._container = document.createElement("div");
        this._container.className = WebInspector.CodeMirrorGradientEditingController.StyleClassName;

        this._gradientTypePicker = this._container.appendChild(document.createElement("select"));
        for (var type in WebInspector.CodeMirrorGradientEditingController.GradientTypes) {
            var option = this._gradientTypePicker.appendChild(document.createElement("option"));
            option.value = type;
            option.innerText = WebInspector.CodeMirrorGradientEditingController.GradientTypes[type].label;
        }
        this._gradientTypePicker.addEventListener("change", this);

        this._gradientSlider = new WebInspector.GradientSlider;
        this._container.appendChild(this._gradientSlider.element);

        this._colorPicker = new WebInspector.ColorPicker;
        this._colorPicker.colorWheel.dimension = 190;
        this._colorPicker.addEventListener(WebInspector.ColorPicker.Event.ColorChanged, this._colorPickerColorChanged, this);

        var angleLabel = this._container.appendChild(document.createElement("label"));
        angleLabel.textContent = WebInspector.UIString("Angle");

        this._angleInput = document.createElement("input");
        this._angleInput.type = "text";
        this._angleInput.size = 3;
        this._angleInput.addEventListener("input", this);
        angleLabel.appendChild(this._angleInput);

        var dragToAdjustController = new WebInspector.DragToAdjustController(this);
        dragToAdjustController.element = angleLabel;
        dragToAdjustController.enabled = true;

        this._updateCSSClassForGradientType();

        popover.content = this._container;
    },

    popoverDidPresent: function(popover)
    {
        this._gradientSlider.stops = this.value.stops;

        if (this.value instanceof WebInspector.LinearGradient) {
            this._gradientTypePicker.value = this.value.repeats ? "repeating-linear-gradient" : "linear-gradient";
            this._angleInput.value = this.value.angle + "\u00B0";
        } else
            this._gradientTypePicker.value = this.value.repeats ? "repeating-radial-gradient" : "radial-gradient"

        this._gradientSlider.delegate = this;
    },

    // Protected

    handleEvent: function(event)
    {
        if (event.type === "input")
            this._handleInputEvent(event);
        else if (event.type === "change")
            this._handleChangeEvent(event);
    },

    gradientSliderStopsDidChange: function(gradientSlider)
    {
        this.text = this.value.toString();
    },
    
    gradientSliderStopWasSelected: function(gradientSlider, stop)
    {
        var selectedStop = gradientSlider.selectedStop;

        if (selectedStop && !this._container.classList.contains(WebInspector.CodeMirrorGradientEditingController.EditsColorClassName)) {
            this._container.appendChild(this._colorPicker.element);
            this._container.classList.add(WebInspector.CodeMirrorGradientEditingController.EditsColorClassName);
            this._colorPicker.color = selectedStop.color;
        } else if (!selectedStop) {
            this._colorPicker.element.remove();
            this._container.classList.remove(WebInspector.CodeMirrorGradientEditingController.EditsColorClassName);
        }

        // Ensure the angle input is not focused since, if it were, it'd make a scrollbar appear as we
        // animate the popover's frame to fit its new content.
        this._angleInput.blur();
        
        this.popover.update();
    },

    dragToAdjustControllerWasAdjustedByAmount: function(dragToAdjustController, amount)
    {
        var angle = parseFloat(this._angleInput.value) + amount;
        if (Math.round(angle) !== angle)
            angle = angle.toFixed(1);

        this._angleInput.value = angle;
        this._angleInputValueDidChange(angle);
    },

    // Private

    _handleInputEvent: function(event)
    {
        var angle = parseFloat(this._angleInput.value);
        if (isNaN(angle))
            return;

        this._angleInputValueDidChange(angle);
    },

    _angleInputValueDidChange: function(angle)
    {
        this.value.angle = angle;
        this.text = this.value.toString();

        var matches = this._angleInput.value.match(/\u00B0/g);
        if (!matches || matches.length !== 1) {
            var selectionStart = this._angleInput.selectionStart;
            this._angleInput.value = angle + "\u00B0";
            this._angleInput.selectionStart = selectionStart;
            this._angleInput.selectionEnd = selectionStart;
        }
    },

    _handleChangeEvent: function(event)
    {
        var descriptor = WebInspector.CodeMirrorGradientEditingController.GradientTypes[this._gradientTypePicker.value];
        if (!(this.value instanceof descriptor.type)) {
            if (descriptor.type === WebInspector.LinearGradient) {
                this.value = new WebInspector.LinearGradient(180, this.value.stops);
                this._angleInput.value = "180\u00B0";
            } else
                this.value = new WebInspector.RadialGradient("", this.value.stops);

            this._updateCSSClassForGradientType();
            this.popover.update();
        }
        this.value.repeats = descriptor.repeats;
        this.text = this.value.toString();
    },

    _colorPickerColorChanged: function(event)
    {
        this._gradientSlider.selectedStop.color = event.target.color;
        this._gradientSlider.stops = this.value.stops;
        this.text = this.value.toString();
    },
    
    _updateCSSClassForGradientType: function()
    {
        if (this.value instanceof WebInspector.LinearGradient)
            this._container.classList.remove(WebInspector.CodeMirrorGradientEditingController.RadialGradientClassName);
        else
            this._container.classList.add(WebInspector.CodeMirrorGradientEditingController.RadialGradientClassName);
    }
}
