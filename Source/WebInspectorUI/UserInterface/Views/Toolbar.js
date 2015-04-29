/*
 * Copyright (C) 2013 Apple Inc. All rights reserved.
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

WebInspector.Toolbar = function(element, navigationItems, dontAllowModeChanges) {
    WebInspector.NavigationBar.call(this, element, navigationItems, "toolbar");

    this.displayMode = WebInspector.Toolbar.DisplayMode.IconAndLabelVertical;
    this.sizeMode = WebInspector.Toolbar.SizeMode.Normal;

    this._controlSectionElement = document.createElement("div");
    this._controlSectionElement.className = WebInspector.Toolbar.ControlSectionStyleClassName;
    this._element.appendChild(this._controlSectionElement);

    this._leftSectionElement = document.createElement("div");
    this._leftSectionElement.className = WebInspector.Toolbar.ItemSectionStyleClassName + " " + WebInspector.Toolbar.LeftItemSectionStyleClassName;
    this._element.appendChild(this._leftSectionElement);

    this._centerLeftSectionElement = document.createElement("div");
    this._centerLeftSectionElement.className = WebInspector.Toolbar.ItemSectionStyleClassName + " " + WebInspector.Toolbar.CenterLeftItemSectionStyleClassName;
    this._element.appendChild(this._centerLeftSectionElement);

    this._centerSectionElement = document.createElement("div");
    this._centerSectionElement.className = WebInspector.Toolbar.ItemSectionStyleClassName + " " + WebInspector.Toolbar.CenterItemSectionStyleClassName;
    this._element.appendChild(this._centerSectionElement);

    this._centerRightSectionElement = document.createElement("div");
    this._centerRightSectionElement.className = WebInspector.Toolbar.ItemSectionStyleClassName + " " + WebInspector.Toolbar.CenterRightItemSectionStyleClassName;
    this._element.appendChild(this._centerRightSectionElement);

    this._rightSectionElement = document.createElement("div");
    this._rightSectionElement.className = WebInspector.Toolbar.ItemSectionStyleClassName + " " + WebInspector.Toolbar.RightItemSectionStyleClassName;
    this._element.appendChild(this._rightSectionElement);

    if (!dontAllowModeChanges)
        this._element.addEventListener("contextmenu", this._handleContextMenuEvent.bind(this), false);
};

// FIXME: Move to a WebInspector.Object subclass and we can remove this.
WebInspector.Object.deprecatedAddConstructorFunctions(WebInspector.Toolbar);

WebInspector.Toolbar.StyleClassName = "toolbar";
WebInspector.Toolbar.ControlSectionStyleClassName = "control-section";
WebInspector.Toolbar.ItemSectionStyleClassName = "item-section";
WebInspector.Toolbar.LeftItemSectionStyleClassName = "left";
WebInspector.Toolbar.CenterLeftItemSectionStyleClassName = "center-left";
WebInspector.Toolbar.CenterItemSectionStyleClassName = "center";
WebInspector.Toolbar.CenterRightItemSectionStyleClassName = "center-right";
WebInspector.Toolbar.RightItemSectionStyleClassName = "right";

WebInspector.Toolbar.Event = {
    DisplayModeDidChange: "toolbar-display-mode-did-change",
    SizeModeDidChange: "toolbar-size-mode-did-change"
};

WebInspector.Toolbar.Section = {
    Control: "control",
    Left: "left",
    CenterLeft: "center-left",
    Center: "center",
    CenterRight: "center-right",
    Right: "right"
};

WebInspector.Toolbar.DisplayMode = {
    IconAndLabelVertical: "icon-and-label-vertical",
    IconAndLabelHorizontal: "icon-and-label-horizontal",
    IconOnly: "icon-only",
    LabelOnly: "label-only"
};

WebInspector.Toolbar.SizeMode = {
    Normal: "normal-size",
    Small: "small-size"
};

WebInspector.Toolbar.prototype = {
    constructor: WebInspector.Toolbar,

    // Public

    get displayMode()
    {
        return this._displayMode;
    },

    set displayMode(mode)
    {
        if (mode === this._displayMode)
            return;

        if (this._displayMode)
            this._element.classList.remove(this._displayMode);

        // Revert the forced icon-only mode if it was applied.
        if (this._displayMode === WebInspector.Toolbar.DisplayMode.IconAndLabelHorizontal)
            this._element.classList.remove(WebInspector.Toolbar.DisplayMode.IconOnly);

        this._displayMode = mode;

        this._element.classList.add(mode);

        this.updateLayout();

        this.dispatchEventToListeners(WebInspector.Toolbar.Event.DisplayModeDidChange);
    },

    get sizeMode()
    {
        return this._sizeMode;
    },

    set sizeMode(mode)
    {
        if (mode === this._sizeMode)
            return;

        if (this._sizeMode)
            this._element.classList.remove(this._sizeMode);

        this._sizeMode = mode;

        this._element.classList.add(mode);

        this.updateLayout();

        this.dispatchEventToListeners(WebInspector.Toolbar.Event.SizeModeDidChange);
    },

    customUpdateLayout: function()
    {
        // Bail early if our sections are not created yet. This means we are being called during construction.
        if (!this._leftSectionElement || !this._centerSectionElement || !this._rightSectionElement)
            return;

        // Force collapsed style for JavaScript debuggables.
        if (WebInspector.debuggableType === WebInspector.DebuggableType.JavaScript) {
            this._element.classList.add(WebInspector.NavigationBar.CollapsedStyleClassName);
            return;
        }

        // Remove the collapsed style class to test if the items can fit at full width.
        this._element.classList.remove(WebInspector.NavigationBar.CollapsedStyleClassName);

        // Revert the forced icon-only mode if it was applied.
        if (this._displayMode === WebInspector.Toolbar.DisplayMode.IconAndLabelHorizontal) {
            this._element.classList.remove(WebInspector.Toolbar.DisplayMode.IconOnly);
            this._element.classList.add(WebInspector.Toolbar.DisplayMode.IconAndLabelHorizontal);
        }

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

        // Only the horizontal display mode supports collapsing labels.
        // If any sections are overflowing the toolbar then force the display mode to be icon only.
        if (this._displayMode === WebInspector.Toolbar.DisplayMode.IconAndLabelHorizontal && isOverflowingToolbar.call(this)) {
            this._element.classList.remove(WebInspector.Toolbar.DisplayMode.IconAndLabelHorizontal);
            this._element.classList.add(WebInspector.Toolbar.DisplayMode.IconOnly);
        }

        if (!isOverflowingToolbar.call(this))
            return;

        this._element.classList.add(WebInspector.NavigationBar.CollapsedStyleClassName);
    },

    addToolbarItem: function(toolbarItem, sectionIdentifier)
    {
        var sectionElement;

        switch (sectionIdentifier) {
        case WebInspector.Toolbar.Section.Control:
            sectionElement = this._controlSectionElement;
            break;

        case WebInspector.Toolbar.Section.Left:
            sectionElement = this._leftSectionElement;
            break;

        case WebInspector.Toolbar.Section.CenterLeft:
            sectionElement = this._centerLeftSectionElement;
            break;

        default:
        case WebInspector.Toolbar.Section.Center:
            sectionElement = this._centerSectionElement;
            break;

        case WebInspector.Toolbar.Section.CenterRight:
            sectionElement = this._centerRightSectionElement;
            break;

        case WebInspector.Toolbar.Section.Right:
            sectionElement = this._rightSectionElement;
            break;
        }

        console.assert(sectionElement);

        this.addNavigationItem(toolbarItem, sectionElement);
    },

    // Private

    _handleContextMenuEvent: function(event)
    {
        var contextMenu = new WebInspector.ContextMenu(event);

        contextMenu.appendCheckboxItem(WebInspector.UIString("Icon and Text (Vertical)"), this._changeDisplayMode.bind(this, WebInspector.Toolbar.DisplayMode.IconAndLabelVertical), this._displayMode === WebInspector.Toolbar.DisplayMode.IconAndLabelVertical);
        contextMenu.appendCheckboxItem(WebInspector.UIString("Icon and Text (Horizontal)"), this._changeDisplayMode.bind(this, WebInspector.Toolbar.DisplayMode.IconAndLabelHorizontal), this._displayMode === WebInspector.Toolbar.DisplayMode.IconAndLabelHorizontal);
        contextMenu.appendCheckboxItem(WebInspector.UIString("Icon Only"), this._changeDisplayMode.bind(this, WebInspector.Toolbar.DisplayMode.IconOnly), this._displayMode === WebInspector.Toolbar.DisplayMode.IconOnly);
        contextMenu.appendCheckboxItem(WebInspector.UIString("Text Only"), this._changeDisplayMode.bind(this, WebInspector.Toolbar.DisplayMode.LabelOnly), this._displayMode === WebInspector.Toolbar.DisplayMode.LabelOnly);

        if (this._displayMode !== WebInspector.Toolbar.DisplayMode.LabelOnly) {
            contextMenu.appendSeparator();
            contextMenu.appendCheckboxItem(WebInspector.UIString("Small Icons"), this._toggleSmallIcons.bind(this), this._sizeMode === WebInspector.Toolbar.SizeMode.Small);
        }

        contextMenu.show();
    },

    _changeDisplayMode: function(displayMode)
    {
        this.displayMode = displayMode;
    },

    _toggleSmallIcons: function()
    {
        this.sizeMode = this._sizeMode === WebInspector.Toolbar.SizeMode.Normal ? WebInspector.Toolbar.SizeMode.Small : WebInspector.Toolbar.SizeMode.Normal;
    }
};

WebInspector.Toolbar.prototype.__proto__ = WebInspector.NavigationBar.prototype;
