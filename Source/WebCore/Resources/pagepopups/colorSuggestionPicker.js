"use strict";
/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

window.argumentsReceived = false;

/**
 * @param {Event} event
 */
function handleMessage(event) {
    initialize(JSON.parse(event.data));
    window.argumentsReceived = true;
}

/**
 * @param {!Object} args
 */
function initialize(args) {
    var main = $("main");
    main.innerHTML = "";
    var errorString = validateArguments(args);
    if (errorString) {
        main.textContent = "Internal error: " + errorString;
        resizeWindow(main.offsetWidth, main.offsetHeight);
    } else
        new ColorPicker(main, args);
}

// The DefaultColorPalette is used when the list of values are empty. 
var DefaultColorPalette = ["#000000", "#404040", "#808080", "#c0c0c0",
    "#ffffff", "#980000", "#ff0000", "#ff9900", "#ffff00", "#00ff00", "#00ffff",
    "#4a86e8", "#0000ff", "#9900ff", "#ff00ff"];

function handleArgumentsTimeout() {
    if (window.argumentsReceived)
        return;
    var args = {
        values : DefaultColorPalette,
        otherColorLabel: "Other..."
    };
    initialize(args);
}

/**
 * @param {!Object} args
 * @return {?string} An error message, or null if the argument has no errors.
 */
function validateArguments(args) {
    if (!args.values)
        return "No values.";
    if (!args.otherColorLabel)
        return "No otherColorLabel.";
    return null;
}

var Actions = {
    ChooseOtherColor: -2,
    Cancel: -1,
    SetValue: 0
};

/**
 * @param {string} value
 */
function submitValue(value) {
    window.pagePopupController.setValueAndClosePopup(Actions.SetValue, value);
}

function handleCancel() {
    window.pagePopupController.setValueAndClosePopup(Actions.Cancel, "");
}

function chooseOtherColor() {
    window.pagePopupController.setValueAndClosePopup(Actions.ChooseOtherColor, "");
}

function ColorPicker(element, config) {
    this._element = element;
    this._config = config;
    if (this._config.values.length === 0)
        this._config.values = DefaultColorPalette;
    this._container = null;
    this._layout();
    document.body.addEventListener("keydown", bind(this._handleKeyDown, this));
    this._element.addEventListener("mousemove", bind(this._handleMouseMove, this));
    this._element.addEventListener("mousedown", bind(this._handleMouseDown, this));
}

var SwatchBorderBoxWidth = 24; // keep in sync with CSS
var SwatchBorderBoxHeight = 24; // keep in sync with CSS
var SwatchesPerRow = 5;
var SwatchesMaxRow = 4;

ColorPicker.prototype._layout = function() {
    var container = createElement("div", "color-swatch-container");
    container.addEventListener("click", bind(this._handleSwatchClick, this), false);
    for (var i = 0; i < this._config.values.length; ++i) {
        var swatch = createElement("button", "color-swatch");
        swatch.dataset.index = i;
        swatch.dataset.value = this._config.values[i];
        swatch.title = this._config.values[i];
        swatch.style.backgroundColor = this._config.values[i];
        container.appendChild(swatch);
    }
    var containerWidth = SwatchBorderBoxWidth * SwatchesPerRow;
    if (this._config.values.length > SwatchesPerRow * SwatchesMaxRow)
        containerWidth += getScrollbarWidth();
    container.style.width = containerWidth + "px";
    container.style.maxHeight = (SwatchBorderBoxHeight * SwatchesMaxRow) + "px";
    this._element.appendChild(container);
    var otherButton = createElement("button", "other-color", this._config.otherColorLabel);
    otherButton.addEventListener("click", chooseOtherColor, false);
    this._element.appendChild(otherButton);
    this._container = container;
    this._otherButton = otherButton;
    var elementWidth = this._element.offsetWidth;
    var elementHeight = this._element.offsetHeight;
    resizeWindow(elementWidth, elementHeight);
};

ColorPicker.prototype.selectColorAtIndex = function(index) {
    index = Math.max(Math.min(this._container.childNodes.length - 1, index), 0);
    this._container.childNodes[index].focus();
};

ColorPicker.prototype._handleMouseMove = function(event) {
    if (event.target.classList.contains("color-swatch"))
        event.target.focus();
};

ColorPicker.prototype._handleMouseDown = function(event) {
    // Prevent blur.
    if (event.target.classList.contains("color-swatch"))
        event.preventDefault();
};

ColorPicker.prototype._handleKeyDown = function(event) {
    var key = event.keyIdentifier;
    if (key === "U+001B") // ESC
        handleCancel();
    else if (key == "Left" || key == "Up" || key == "Right" || key == "Down") {
        var selectedElement = document.activeElement;
        var index = 0;
        if (selectedElement.classList.contains("other-color")) {
            if (key != "Right" && key != "Up")
                return;
            index = this._container.childNodes.length - 1;
        } else if (selectedElement.classList.contains("color-swatch")) {
            index = parseInt(selectedElement.dataset.index, 10);
            switch (key) {
            case "Left":
                index--;
                break;
            case "Right":
                index++;
                break;
            case "Up":
                index -= SwatchesPerRow;
                break;
            case "Down":
                index += SwatchesPerRow;
                break;
            }
            if (index > this._container.childNodes.length - 1) {
                this._otherButton.focus();
                return;
            }
        }
        this.selectColorAtIndex(index);
    }
    event.preventDefault();
};

ColorPicker.prototype._handleSwatchClick = function(event) {
    if (event.target.classList.contains("color-swatch"))
        submitValue(event.target.dataset.value);
};

if (window.dialogArguments) {
    initialize(dialogArguments);
} else {
    window.addEventListener("message", handleMessage, false);
    window.setTimeout(handleArgumentsTimeout, 1000);
}
