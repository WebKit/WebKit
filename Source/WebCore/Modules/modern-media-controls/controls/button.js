/*
 * Copyright (C) 2016-2017 Apple Inc. All Rights Reserved.
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

class Button extends LayoutItem
{

    constructor({ layoutDelegate = null, cssClassName = "", iconName = "" } = {})
    {
        super({
            element: "<button />",
            layoutDelegate
        });

        if (!!cssClassName)
            this.element.classList.add(cssClassName);

        this.style = Button.Styles.Bar;
        this.image = this.addChild(new LayoutNode(`<picture></picture>`));

        this._scaleFactor = 1;
        this._imageSource = null;
        this._iconName = "";

        if (!!iconName)
            this.iconName = iconName;

        this._enabled = true;

        if (GestureRecognizer.SupportsTouches)
            this._tapGestureRecognizer = new TapGestureRecognizer(this.element, this);
        else
            this.element.addEventListener("click", this);
    }

    // Public

    get enabled()
    {
        return this._enabled;
    }

    set enabled(flag)
    {
        if (this._enabled === flag)
            return;

        this._enabled = flag;
        if (this.layoutDelegate && typeof this.layoutDelegate.layout === "function")
            this.layoutDelegate.layout();
    }

    get iconName()
    {
        return this._iconName;
    }

    set iconName(iconName)
    {
        if (this._iconName === iconName)
            return;

        this._loadImage(iconName);
        this.element.setAttribute("aria-label", iconName.label);
    }

    get on()
    {
        return this.element.classList.contains("on");
    }

    set on(flag) {
        this.element.classList.toggle("on", flag);
    }

    get style()
    {
        return this._style;
    }

    set style(style)
    {
        if (style === this._style)
            return;

        this.element.classList.remove(this._style);
        this.element.classList.add(style);

        this._style = style;

        if (style === Button.Styles.Bar && this.children.length == 2)
            this.children[0].remove();
        else if (this.children.length == 1)
            this.addChild(new BackgroundTint, 0);
    }

    get scaleFactor()
    {
        return this._scaleFactor;
    }

    set scaleFactor(scaleFactor)
    {
        if (this._scaleFactor === scaleFactor)
            return;

        this._scaleFactor = scaleFactor;
        this._updateImageMetrics();
    }

    // Protected

    handleEvent(event)
    {
        if (event.target === this._imageSource) {
            if (event.type === "load")
                this._imageSourceDidLoad();
            else if (event.type === "error")
                console.error(`Button failed to load, iconName = ${this._iconName.name}, layoutTraits = ${this.layoutTraits}, src = ${this._imageSource.src}`);
        } else if (event.type === "click" && event.currentTarget === this.element)
            this._notifyDelegateOfActivation();
    }

    gestureRecognizerStateDidChange(recognizer)
    {
        if (this._tapGestureRecognizer === recognizer && recognizer.state === GestureRecognizer.States.Recognized)
            this._notifyDelegateOfActivation();
    }

    commitProperty(propertyName)
    {
        if (propertyName === "maskImage")
            this.image.element.style.webkitMaskImage = `url(${this._imageSource.src})`;
        else
            super.commitProperty(propertyName);
    }

    // Private

    _notifyDelegateOfActivation()
    {
        if (this._enabled && this.uiDelegate && typeof this.uiDelegate.buttonWasPressed === "function")
            this.uiDelegate.buttonWasPressed(this);
    }

    _loadImage(iconName)
    {
        if (this._imageSource)
            this._imageSource.removeEventListener("load", this);

        this._imageSource = iconService.imageForIconAndLayoutTraits(iconName, this.layoutTraits);

        this._iconName = iconName;

        if (this._imageSource.complete)
            this._updateImage();
        else {
            this._imageSource.addEventListener("load", this);
            this._imageSource.addEventListener("error", this);
        }
    }

    _imageSourceDidLoad()
    {
        this._imageSource.removeEventListener("load", this);
        this._updateImage();
    }

    _updateImage()
    {
        this.markDirtyProperty("maskImage");

        this._updateImageMetrics();
    }

    _updateImageMetrics()
    {
        let width = this._imageSource.width * this._scaleFactor;
        let height = this._imageSource.height * this._scaleFactor;

        if (this._iconName.type === "png" || this._iconName.type === "pdf") {
            width /= window.devicePixelRatio;
            height /= window.devicePixelRatio;
        }

        if (this.image.width === width && this.image.height === height)
            return;

        this.image.width = width;
        this.image.height = height;

        this.width = width;
        this.height = height;

        if (this.layoutDelegate)
            this.layoutDelegate.needsLayout = true;
    }

}

Button.Styles = {
    Bar: "bar",
    Corner: "corner",
    Center: "center",
    SmallCenter: "small-center"
};
