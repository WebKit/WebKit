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

class IconButton extends Button
{

    constructor({ layoutDelegate = null, cssClassName = "", iconName = "" } = {})
    {
        super(layoutDelegate);

        this.element.classList.add("icon");
        if (!!cssClassName)
            this.element.classList.add(cssClassName);

        this._image = null;
        this._iconName = "";
        this._iconLayoutTraits = LayoutTraits.Unknown;

        if (!!iconName)
            this.iconName = iconName;
    }

    // Public

    get iconName()
    {
        return this._iconName;
    }

    set iconName(iconName)
    {
        if (this._iconName === iconName)
            return;

        this._loadImage(iconName);
    }

    get on()
    {
        return this.element.classList.contains("on");
    }

    set on(flag) {
        this.element.classList.toggle("on", flag);
    }

    layoutTraitsDidChange()
    {
        if (this._iconLayoutTraits !== this.layoutTraits)
            this._loadImage(this._iconName);
    }

    // Protected

    handleEvent(event)
    {
        if (event.type === "load" && event.target === this._image)
            this._imageDidLoad();
        else
            super.handleEvent(event);
    }

    layout()
    {
        super.layout();

        this.element.style.webkitMaskImage = `url(${this._image.src})`;
        this.element.style.webkitMaskSize = `${this.width}px ${this.height}px`;
    }

    // Private

    _loadImage(iconName)
    {
        if (this._image)
            this._image.removeEventListener("load", this);

        this._iconLayoutTraits = this.layoutTraits;
        this._image = iconService.imageForIconNameAndLayoutTraits(iconName, this._iconLayoutTraits);

        this._iconName = iconName;

        if (this._image.complete)
            this._updateImage();
        else
            this._image.addEventListener("load", this);
    }

    _imageDidLoad()
    {
        this._image.removeEventListener("load", this);
        this._updateImage();
    }

    _updateImage()
    {
        this.needsLayout = true;

        const width = this._image.width / window.devicePixelRatio;
        const height = this._image.height / window.devicePixelRatio;

        if (this.width === width && this.height === height)
            return;

        this.width = width;
        this.height = height;

        if (this.layoutDelegate)
            this.layoutDelegate.needsLayout = true;
    }

}
