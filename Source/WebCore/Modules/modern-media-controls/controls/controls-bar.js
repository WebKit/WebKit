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

class ControlsBar extends LayoutNode
{

    constructor(cssClassName = "")
    {
        super(`<div role="group" class="controls-bar ${cssClassName}"></div>`);
        this._hasBackgroundTint = true;
        this._translation = new DOMPoint;
        this._backgroundTint = this.addChild(new BackgroundTint);
    }

    // Public

    get hasBackgroundTint()
    {
        return this._hasBackgroundTint;
    }

    set hasBackgroundTint(hasBackgroundTint)
    {
        if (this._hasBackgroundTint == hasBackgroundTint)
            return;

        this._hasBackgroundTint = hasBackgroundTint;

        if (hasBackgroundTint) {
            console.assert(!this._backgroundTint.parent);
            this.addChild(this._backgroundTint, 0);
        } else {
            console.assert(this._backgroundTint.parent === this);
            this._backgroundTint.remove();
        }
    }

    get children()
    {
        return super.children;
    }

    set children(children)
    {
        if (this._hasBackgroundTint)
            super.children = [this._backgroundTint].concat(children);
        else
            super.children = children;
    }

    get translation()
    {
        return new DOMPoint(this._translation.x, this._translation.y);
    }

    set translation(point)
    {
        if (this._translation.x === point.x && this._translation.y === point.y)
            return;

        this._translation = new DOMPoint(point.x, point.y);
        this.markDirtyProperty("translation");
    }

    // Protected

    commitProperty(propertyName)
    {
        if (propertyName === "translation")
            this.element.style.transform = `translate(${this._translation.x}px, ${this._translation.y}px)`;
        else
            super.commitProperty(propertyName);
    }

}
