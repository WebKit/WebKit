/*
 * Copyright (C) 2021 Apple Inc. All Rights Reserved.
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

class OverflowButton extends Button
{

    constructor(layoutDelegate)
    {
        super({
            cssClassName: "overflow",
            iconName: Icons.Overflow,
            layoutDelegate,
        });

        this.clearContextMenuOptions();
        this._defaultContextMenuOptions = {};
    }

    // Public

    get visible()
    {
        return super.visible;
    }

    set visible(flag)
    {
        let hasContextMenuOptions = false;
        for (let key in this._contextMenuOptions) {
            hasContextMenuOptions = true;
            break;
        }

        super.visible = flag && hasContextMenuOptions;
    }

    get contextMenuOptions()
    {
        return this._contextMenuOptions;
    }

    addContextMenuOptions(contextMenuOptions)
    {
        if (!this.enabled)
            return;

        for (let key in contextMenuOptions)
            this._contextMenuOptions[key] = contextMenuOptions[key];

        this.visible = true;
    }

    clearContextMenuOptions()
    {
        this._contextMenuOptions = {};

        this.visible = false;

        this.addContextMenuOptions(this._defaultContextMenuOptions);
    }

    set defaultContextMenuOptions(defaultContextMenuOptions)
    {
        this._defaultContextMenuOptions = defaultContextMenuOptions || {};

        this.clearContextMenuOptions();

        if (this.layoutDelegate)
            this.layoutDelegate.needsLayout = true;
    }

}
