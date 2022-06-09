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

class ButtonsContainer extends LayoutNode
{

    constructor({ children = [], leftMargin = 16, rightMargin = 16, buttonMargin = 16, cssClassName = "" } = {})
    {
        super(`<div class="buttons-container ${cssClassName}"></div>`);

        this.leftMargin = leftMargin;
        this.rightMargin = rightMargin;
        this.buttonMargin = buttonMargin;
        this.children = children;
    }

    // Public

    willRemoveChild(child)
    {
        super.willRemoveChild(child);

        // We reset properties that we may have overridden during layout to their default values.
        child.visible = true;
        child.x = 0;
    }

    didChangeChildren()
    {
        super.didChangeChildren();
        this.layout();
    }

    layout()
    {
        super.layout();

        let x = this.leftMargin;
        let numberOfVisibleButtons = 0;

        this._children.forEach(button => {
            button.visible = button.enabled && !button.dropped;
            if (!button.visible)
                return;

            button.x = x;
            x += button.width + this.buttonMargin;
            numberOfVisibleButtons++;
        });

        if (numberOfVisibleButtons)
            this.width = x - this.buttonMargin + this.rightMargin;
        else
            this.width = this.buttonMargin + this.rightMargin;
    }

}
