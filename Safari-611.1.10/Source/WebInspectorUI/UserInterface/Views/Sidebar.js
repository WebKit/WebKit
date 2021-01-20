/*
 * Copyright (C) 2013, 2015, 2020 Apple Inc. All rights reserved.
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

WI.Sidebar = class Sidebar extends WI.View
{
    constructor(element, side, label)
    {
        super(element);

        // This class should not be instantiated directly. Create a concrete subclass instead.
        console.assert(this.constructor !== WI.Sidebar, this);

        console.assert(Object.values(WI.Sidebar.Sides).includes(side), side);
        this._side = side;

        this._collapsed = true;
        this._collapsable = true;

        this.element.classList.add("sidebar", this._side, WI.Sidebar.CollapsedStyleClassName);

        this.element.setAttribute("role", "group");
        if (label)
            this.element.setAttribute("aria-label", label);

        this._sidebarPanels = [];
        this._selectedSidebarPanel = null;
    }

    // Public

    get sidebarPanels() { return this._sidebarPanels; }
    get side() { return this._side; }

    addSidebarPanel(sidebarPanel)
    {
        this.insertSidebarPanel(sidebarPanel, this.sidebarPanels.length);
    }

    insertSidebarPanel(sidebarPanel, index)
    {
        console.assert(sidebarPanel instanceof WI.SidebarPanel);
        if (!(sidebarPanel instanceof WI.SidebarPanel))
            return;

        if (!this.shouldInsertSidebarPanel(sidebarPanel, index))
            return;

        console.assert(index >= 0 && index <= this.sidebarPanels.length);
        this._sidebarPanels.splice(index, 0, sidebarPanel);

        this.didInsertSidebarPanel(sidebarPanel, index);
    }

    removeSidebarPanel(sidebarPanelOrIdentifierOrIndex)
    {
        let sidebarPanel = this._findSidebarPanel(sidebarPanelOrIdentifierOrIndex);
        if (!sidebarPanel)
            return;

        this._sidebarPanels.remove(sidebarPanel);

        if (this.selectedSidebarPanel === sidebarPanel)
            this.selectedSidebarPanel = 0;

        this.didRemoveSidebarPanel(sidebarPanel);
    }

    get selectedSidebarPanel()
    {
        return this._selectedSidebarPanel;
    }

    set selectedSidebarPanel(sidebarPanelOrIdentifierOrIndex)
    {
        let sidebarPanel = this._findSidebarPanel(sidebarPanelOrIdentifierOrIndex);
        if (!sidebarPanel)
            sidebarPanel = this._findSidebarPanel(0);

        if (this._selectedSidebarPanel === sidebarPanel)
            return;

        this.willSetSelectedSidebarPanel(sidebarPanel);

        this._selectedSidebarPanel = sidebarPanel;

        this.didSetSelectedSidebarPanel(sidebarPanel);

        this.dispatchEventToListeners(WI.Sidebar.Event.SidebarPanelSelected);
    }

    get collapsed()
    {
        return this._collapsed;
    }

    set collapsed(flag)
    {
        if (flag === this._collapsed)
            return;

        if (flag && !this._collapsable)
            return;

        this._collapsed = flag || false;
        this.element.classList.toggle(WI.Sidebar.CollapsedStyleClassName);

        this.didSetCollapsed();

        this.dispatchEventToListeners(WI.Sidebar.Event.CollapsedStateDidChange);
    }

    get collapsable()
    {
        return this._collapsable;
    }

    set collapsable(allow) {
        if (allow === this._collapsable)
            return;

        this._collapsable = !!allow;

        if (!allow && this.collapsed)
            this.collapsed = false;
    }

    get minimumWidth()
    {
        let minimumWidth = WI.Sidebar.AbsoluteMinimumWidth;
        if (this.selectedSidebarPanel)
            minimumWidth = Math.max(minimumWidth, this.selectedSidebarPanel.minimumWidth);
        return minimumWidth;
    }

    get maximumWidth()
    {
        return WI.getMaximumSidebarWidth(this);
    }

    // Protected

    shouldInsertSidebarPanel(sidebarPanel, index)
    {
        // Implemented by subclasses if needed.
        return true;
    }

    didInsertSidebarPanel(sidebarPanel, index)
    {
        // Implemented by subclasses if needed.
    }

    didRemoveSidebarPanel(sidebarPanel)
    {
        // Implemented by subclasses if needed.
    }

    willSetSelectedSidebarPanel(sidebarPanel)
    {
        // Implemented by subclasses if needed.
    }

    didSetSelectedSidebarPanel(sidebarPanel)
    {
        // Implemented by subclasses if needed.
    }

    didSetCollapsed(flag)
    {
        // Implemented by subclasses if needed.
    }

    // Private

    _findSidebarPanel(sidebarPanelOrIdentifierOrIndex)
    {
        let sidebarPanel = null;

        if (sidebarPanelOrIdentifierOrIndex instanceof WI.SidebarPanel) {
            if (this._sidebarPanels.includes(sidebarPanelOrIdentifierOrIndex))
                sidebarPanel = sidebarPanelOrIdentifierOrIndex;
        } else if (typeof sidebarPanelOrIdentifierOrIndex === "number") {
            sidebarPanel = this._sidebarPanels[sidebarPanelOrIdentifierOrIndex];
        } else if (typeof sidebarPanelOrIdentifierOrIndex === "string") {
            sidebarPanel = this._sidebarPanels.find((existingSidebarPanel) => existingSidebarPanel.identifier === sidebarPanelOrIdentifierOrIndex) || null;
        }

        return sidebarPanel;
    }
};

WI.Sidebar.CollapsedStyleClassName = "collapsed";
WI.Sidebar.AbsoluteMinimumWidth = 250; // Keep in sync with `#details-sidebar` in `Main.css`

WI.Sidebar.Sides = {
    Leading: "leading",
    Trailing: "trailing",
};

WI.Sidebar.Event = {
    SidebarPanelSelected: "sidebar-panel-selected",
    CollapsedStateDidChange: "sidebar-collapsed-state-did-change",
    WidthDidChange: "sidebar-width-did-change",
};
