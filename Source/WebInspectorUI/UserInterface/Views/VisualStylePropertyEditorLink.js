/*
 * Copyright (C) 2015 Apple Inc. All rights reserved.
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

WebInspector.VisualStylePropertyEditorLink = class VisualStylePropertyEditorLink extends WebInspector.Object
{
    constructor(linkedProperties, className, linksToHideWhenLinked)
    {
        super();

        this._linkedProperties = linkedProperties || [];
        this._linksToHideWhenLinked = linksToHideWhenLinked || [];
        this._lastPropertyEdited = null;

        for (let property of this._linkedProperties)
            property.addEventListener(WebInspector.VisualStylePropertyEditor.Event.ValueDidChange, this._linkedPropertyValueChanged, this);

        this._element = document.createElement("div");
        this._element.classList.add("visual-style-property-editor-link", className || "");

        let leftLineElement = document.createElement("div");
        leftLineElement.classList.add("visual-style-property-editor-link-border", "left");
        this._element.appendChild(leftLineElement);

        this._iconElement = document.createElement("div");
        this._iconElement.classList.add("visual-style-property-editor-link-icon");
        this._iconElement.title = WebInspector.UIString("Click to link property values");
        this._iconElement.addEventListener("mouseover", this._iconMouseover.bind(this));
        this._iconElement.addEventListener("mouseout", this._iconMouseout.bind(this));
        this._iconElement.addEventListener("click", this._iconClicked.bind(this));

        this._unlinkedIcon = useSVGSymbol("Images/VisualStylePropertyUnlinked.svg", "unlinked-icon");
        this._iconElement.appendChild(this._unlinkedIcon);

        this._linkedIcon = useSVGSymbol("Images/VisualStylePropertyLinked.svg", "linked-icon");
        this._linkedIcon.hidden = true;
        this._iconElement.appendChild(this._linkedIcon);

        this._element.appendChild(this._iconElement);

        let rightLineElement = document.createElement("div");
        rightLineElement.classList.add("visual-style-property-editor-link-border", "right");
        this._element.appendChild(rightLineElement);

        this._linked = false;
        this._disabled = false;
    }

    // Public

    get element()
    {
        return this._element;
    }

    set linked(flag)
    {
        this._linked = flag;
        this._element.classList.toggle("linked", this._linked);

        if (this._linkedIcon)
            this._linkedIcon.hidden = !this._linked;

        if (this._unlinkedIcon)
            this._unlinkedIcon.hidden = this._linked;

        this._iconElement.title = this._linked ? WebInspector.UIString("Remove link") : WebInspector.UIString("Link property values");

        for (let linkToHide of this._linksToHideWhenLinked)
            linkToHide.disabled = this._linked;
    }

    set disabled(flag)
    {
        this._disabled = flag;
        this._element.classList.toggle("disabled", this._disabled);
    }

    // Private

    _linkedPropertyValueChanged(event)
    {
        if (!event)
            return;

        let property = event.target;
        if (!property)
            return;

        this._lastPropertyEdited = property;
        if (!this._linked)
            return;

        this._updateLinkedEditors(property);
    }

    _updateLinkedEditors(property)
    {
        let style = property.style;
        let text = style.text;
        let value = property.synthesizedValue || null;

        for (let linkedProperty of this._linkedProperties)
            text = linkedProperty.updateValueFromText(text, value);

        style.text = text;
    }

    _iconMouseover()
    {
        this._linkedIcon.hidden = this._linked;
        this._unlinkedIcon.hidden = !this._linked;
    }

    _iconMouseout()
    {
        this._linkedIcon.hidden = !this._linked;
        this._unlinkedIcon.hidden = this._linked;
    }

    _iconClicked()
    {
        this.linked = !this._linked;
        this._updateLinkedEditors(this._lastPropertyEdited || this._linkedProperties[0]);
    }
};
