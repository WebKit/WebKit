/*
 * Copyright (C) 2013-2017 Apple Inc. All rights reserved.
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

WI.Toolbar = class Toolbar extends WI.NavigationBar
{
    constructor(element)
    {
        super(element, null, "toolbar");

        this._controlSectionElement = document.createElement("div");
        this._controlSectionElement.className = WI.Toolbar.ControlSectionStyleClassName;
        this.element.appendChild(this._controlSectionElement);

        this._leftSectionElement = document.createElement("div");
        this._leftSectionElement.className = WI.Toolbar.ItemSectionStyleClassName + " " + WI.Toolbar.LeftItemSectionStyleClassName;
        this.element.appendChild(this._leftSectionElement);

        this._centerLeftSectionElement = document.createElement("div");
        this._centerLeftSectionElement.className = WI.Toolbar.ItemSectionStyleClassName + " " + WI.Toolbar.CenterLeftItemSectionStyleClassName;
        this.element.appendChild(this._centerLeftSectionElement);

        this._centerSectionElement = document.createElement("div");
        this._centerSectionElement.className = WI.Toolbar.ItemSectionStyleClassName + " " + WI.Toolbar.CenterItemSectionStyleClassName;
        this.element.appendChild(this._centerSectionElement);

        this._centerRightSectionElement = document.createElement("div");
        this._centerRightSectionElement.className = WI.Toolbar.ItemSectionStyleClassName + " " + WI.Toolbar.CenterRightItemSectionStyleClassName;
        this.element.appendChild(this._centerRightSectionElement);

        this._rightSectionElement = document.createElement("div");
        this._rightSectionElement.className = WI.Toolbar.ItemSectionStyleClassName + " " + WI.Toolbar.RightItemSectionStyleClassName;
        this.element.appendChild(this._rightSectionElement);
    }

    // Public

    addToolbarItem(toolbarItem, sectionIdentifier)
    {
        var sectionElement;

        switch (sectionIdentifier) {
        case WI.Toolbar.Section.Control:
            sectionElement = this._controlSectionElement;
            break;

        case WI.Toolbar.Section.Left:
            sectionElement = this._leftSectionElement;
            break;

        case WI.Toolbar.Section.CenterLeft:
            sectionElement = this._centerLeftSectionElement;
            break;

        default:
        case WI.Toolbar.Section.Center:
            sectionElement = this._centerSectionElement;
            break;

        case WI.Toolbar.Section.CenterRight:
            sectionElement = this._centerRightSectionElement;
            break;

        case WI.Toolbar.Section.Right:
            sectionElement = this._rightSectionElement;
            break;
        }

        console.assert(sectionElement);

        this.addNavigationItem(toolbarItem, sectionElement);
    }

    // Protected

    layout()
    {
        // Bail early if our sections are not created yet. This means we are being called during construction.
        if (!this._leftSectionElement || !this._centerSectionElement || !this._rightSectionElement)
            return;

        // Force collapsed style for non-Web debuggables.
        if (!WI.sharedApp.isWebDebuggable()) {
            this.element.classList.add(WI.NavigationBar.CollapsedStyleClassName);
            return;
        }

        // Remove the collapsed style class to test if the items can fit at full width.
        this.element.classList.remove(WI.NavigationBar.CollapsedStyleClassName);

        function isOverflowingToolbar()
        {
            var controlSectionWidth = this._controlSectionElement.realOffsetWidth;
            var leftSectionWidth = this._leftSectionElement.realOffsetWidth;
            var centerLeftSectionWidth = this._centerLeftSectionElement.realOffsetWidth;
            var centerSectionWidth = this._centerSectionElement.realOffsetWidth;
            var centerRightSectionWidth = this._centerRightSectionElement.realOffsetWidth;
            var rightSectionWidth = this._rightSectionElement.realOffsetWidth;

            var toolbarWidth = Math.round(this.element.realOffsetWidth);
            return Math.round(controlSectionWidth + leftSectionWidth + centerLeftSectionWidth + centerSectionWidth + centerRightSectionWidth + rightSectionWidth) > toolbarWidth;
        }

        if (!isOverflowingToolbar.call(this))
            return;

        this.element.classList.add(WI.NavigationBar.CollapsedStyleClassName);
    }
};

WI.Toolbar.StyleClassName = "toolbar";
WI.Toolbar.ControlSectionStyleClassName = "control-section";
WI.Toolbar.ItemSectionStyleClassName = "item-section";
WI.Toolbar.LeftItemSectionStyleClassName = "left";
WI.Toolbar.CenterLeftItemSectionStyleClassName = "center-left";
WI.Toolbar.CenterItemSectionStyleClassName = "center";
WI.Toolbar.CenterRightItemSectionStyleClassName = "center-right";
WI.Toolbar.RightItemSectionStyleClassName = "right";

WI.Toolbar.Section = {
    Control: "control",
    Left: "left",
    CenterLeft: "center-left",
    Center: "center",
    CenterRight: "center-right",
    Right: "right"
};
