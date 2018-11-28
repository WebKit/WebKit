/*
 * Copyright (C) 2017 Apple Inc. All rights reserved.
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

WI.ShaderProgramTreeElement = class ShaderProgramTreeElement extends WI.GeneralTreeElement
{
    constructor(shaderProgram)
    {
        console.assert(shaderProgram instanceof WI.ShaderProgram);

        const subtitle = null;
        super("shader-program", shaderProgram.displayName, subtitle, shaderProgram);

        this._disabledImageElement = document.createElement("img");
        this._disabledImageElement.title = WI.UIString("Disable Program");
        this._disabledImageElement.addEventListener("click", this._disabledImageElementClicked.bind(this));
        this.status = this._disabledImageElement;
    }

    // Protected

    onattach()
    {
        super.onattach();

        this.element.addEventListener("mouseover", this._handleMouseOver.bind(this));
        this.element.addEventListener("mouseout", this._handleMouseOut.bind(this));
    }

    canSelectOnMouseDown(event)
    {
        return !this._statusElement.contains(event.target);
    }

    // Private

    _disabledImageElementClicked(event)
    {
        this.representedObject.toggleDisabled(() => {
            this._listItemNode.classList.toggle("disabled", !!this.representedObject.disabled);
            this._disabledImageElement.title = this.representedObject.disabled ? WI.UIString("Enable Program") : WI.UIString("Disable Program");
        });
    }

    _handleMouseOver(event)
    {
        this.representedObject.showHighlight();
    }

    _handleMouseOut(event)
    {
        this.representedObject.hideHighlight();
    }
};
