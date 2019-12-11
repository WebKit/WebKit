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

WI.ActivateButtonNavigationItem = class ActivateButtonNavigationItem extends WI.ButtonNavigationItem
{
    constructor(identifier, defaultToolTip, activatedToolTip, image, imageWidth, imageHeight, role)
    {
        super(identifier, defaultToolTip, image, imageWidth, imageHeight, role);

        this._defaultToolTip = defaultToolTip;
        this._activatedToolTip = activatedToolTip || defaultToolTip;
        this._role = role;
    }

    // Public

    get defaultToolTip()
    {
        return this._defaultToolTip;
    }

    get activatedToolTip()
    {
        return this._activatedToolTip;
    }

    get activated()
    {
        return this.element.classList.contains(WI.ActivateButtonNavigationItem.ActivatedStyleClassName);
    }

    set activated(flag)
    {
        this.element.classList.toggle(WI.ActivateButtonNavigationItem.ActivatedStyleClassName, flag);

        if (flag) {
            this.tooltip = this._activatedToolTip;
            if (this._role === "tab")
                this.element.setAttribute("aria-selected", "true");
        } else {
            this.tooltip = this._defaultToolTip;
            if (this._role === "tab")
                this.element.removeAttribute("aria-selected");
        }
    }

    // Protected

    get additionalClassNames()
    {
        return ["activate", "button"];
    }
};

WI.ActivateButtonNavigationItem.ActivatedStyleClassName = "activated";
