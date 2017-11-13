/*
 * Copyright (C) 2013, 2015 Apple Inc. All rights reserved.
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

WI.ButtonNavigationItem = class ButtonNavigationItem extends WI.NavigationItem
{
    constructor(identifier, toolTipOrLabel, image, imageWidth, imageHeight, role, label)
    {
        super(identifier);

        console.assert(identifier);
        console.assert(toolTipOrLabel);

        this._enabled = true;

        this.toolTip = toolTipOrLabel;

        this.element.addEventListener("click", this._mouseClicked.bind(this));

        this.element.setAttribute("role", role || "button");

        if (label)
            this.element.setAttribute("aria-label", label);

        this._imageWidth = imageWidth || 16;
        this._imageHeight = imageHeight || 16;
        this._label = "";

        if (image)
            this.image = image;
        else if (toolTipOrLabel)
            this.label = toolTipOrLabel;
    }

    // Public

    get toolTip()
    {
        return this.element.title;
    }

    set toolTip(newToolTip)
    {
        console.assert(newToolTip);
        if (!newToolTip)
            return;

        this.element.title = newToolTip;
    }

    get label()
    {
        return this._label;
    }

    set label(newLabel)
    {
        newLabel = newLabel || "";
        if (this._label === newLabel)
            return;

        this.element.classList.add(WI.ButtonNavigationItem.TextOnlyClassName);
        this._label = newLabel;

        this.updateButtonText();
        if (this.parentNavigationBar)
            this.parentNavigationBar.needsLayout();
    }

    get image()
    {
        return this._image;
    }

    set image(newImage)
    {
        if (!newImage) {
            this.element.removeChildren();
            return;
        }

        this.element.removeChildren();
        this.element.classList.remove(WI.ButtonNavigationItem.TextOnlyClassName);

        this._image = newImage;

        this._glyphElement = WI.ImageUtilities.useSVGSymbol(this._image, "glyph");
        this._glyphElement.style.width = this._imageWidth + "px";
        this._glyphElement.style.height = this._imageHeight + "px";
        this.element.appendChild(this._glyphElement);
    }

    get enabled()
    {
        return this._enabled;
    }

    set enabled(flag)
    {
        flag = !!flag;
        if (this._enabled === flag)
            return;

        this._enabled = flag;
        this.element.classList.toggle("disabled", !this._enabled);
    }

    // Protected

    get additionalClassNames()
    {
        return ["button"];
    }

    updateButtonText()
    {
        // Overridden by subclasses.

        this.element.textContent = this._label;
    }

    // Private

    _mouseClicked(event)
    {
        if (!this.enabled)
            return;
        this.dispatchEventToListeners(WI.ButtonNavigationItem.Event.Clicked, {nativeEvent: event});
    }
};

WI.ButtonNavigationItem.TextOnlyClassName = "text-only";

WI.ButtonNavigationItem.Event = {
    Clicked: "button-navigation-item-clicked"
};
