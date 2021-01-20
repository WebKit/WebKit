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

const MinHeightToDisplayDescription = 100;
const MinHeightToDisplayTitle = 40;

class Placard extends LayoutItem
{

    constructor({ iconName = null, title = "", description = "", width = 400, height = 300, layoutDelegate = null } = {})
    {
        super({
            element: `<div class="placard"></div>`,
            layoutDelegate
        });

        this._container = this.addChild(new LayoutNode(`<div class="container"></div>`));
        
        if (iconName) {
            this._icon = new Button(this);
            this._icon.iconName = iconName;
            this._icon.element.disabled = true;
        }

        if (!!title)
            this._titleNode = new LayoutNode(`<div class="title">${title}</div>`);

        if (!!description)
            this._descriptionNode = new LayoutNode(`<div class="description">${description}</div>`);

        this.minDimensionToDisplayIcon = 170;

        this.width = width;
        this.height = height;
    }

    // Protected

    layout()
    {
        super.layout();

        const children = [];

        if (this._icon && this.width >= this.minDimensionToDisplayIcon && this.height >= this.minDimensionToDisplayIcon)
            children.push(this._icon);

        if (this._titleNode && this.height >= MinHeightToDisplayTitle)
            children.push(this._titleNode);

        if (this._descriptionNode && this.height >= MinHeightToDisplayDescription)
            children.push(this._descriptionNode);

        this._container.children = children;
    }
    
    set description(description)
    {
        this._descriptionNode = !!description ? new LayoutNode(`<div class="description">${description}</div>`) : null;
        this.needsLayout = true;
    }

}
