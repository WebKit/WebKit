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

        this.toolTip = toolTipOrLabel;

        this._element.addEventListener("click", this._mouseClicked.bind(this));

        this._element.setAttribute("role", role || "button");

        if (label)
            this._element.setAttribute("aria-label", label);

        this._imageWidth = imageWidth || 16;
        this._imageHeight = imageHeight || 16;

        if (image)
            this.image = image;
        else
            this.label = toolTipOrLabel;
    }

    // Public

    get toolTip()
    {
        return this._element.title;
    }

    set toolTip(newToolTip)
    {
        console.assert(newToolTip);
        if (!newToolTip)
            return;

        this._element.title = newToolTip;
    }

    get label()
    {
        return this._element.textContent;
    }

    set label(newLabel)
    {
        this._element.classList.add(WI.ButtonNavigationItem.TextOnlyClassName);
        this._element.textContent = newLabel || "";
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
            this._element.removeChildren();
            return;
        }

        this._element.removeChildren();
        this._element.classList.remove(WI.ButtonNavigationItem.TextOnlyClassName);

        this._image = newImage;

        this._glyphElement = useSVGSymbol(this._image, "glyph");
        this._glyphElement.style.width = this._imageWidth + "px";
        this._glyphElement.style.height = this._imageHeight + "px";
        this._element.appendChild(this._glyphElement);
    }

    get enabled()
    {
        return !this._element.classList.contains(WI.ButtonNavigationItem.DisabledStyleClassName);
    }

    set enabled(flag)
    {
        if (flag)
            this._element.classList.remove(WI.ButtonNavigationItem.DisabledStyleClassName);
        else
            this._element.classList.add(WI.ButtonNavigationItem.DisabledStyleClassName);
    }

    // Protected

    get additionalClassNames()
    {
        return ["button"];
    }

    // Private

    _mouseClicked(event)
    {
        if (!this.enabled)
            return;
        this.dispatchEventToListeners(WI.ButtonNavigationItem.Event.Clicked, {nativeEvent: event});
    }
};

WI.ButtonNavigationItem.DisabledStyleClassName = "disabled";
WI.ButtonNavigationItem.TextOnlyClassName = "text-only";

WI.ButtonNavigationItem.Event = {
    Clicked: "button-navigation-item-clicked"
};
