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

WI.Sidebar = class Sidebar extends WI.View
{
    constructor(element, side, sidebarPanels, role, label, hasNavigationBar)
    {
        super(element);

        console.assert(!side || side === WI.Sidebar.Sides.Left || side === WI.Sidebar.Sides.Right);
        this._side = side || WI.Sidebar.Sides.Left;
        this._collapsed = true;

        this.element.classList.add("sidebar", this._side, WI.Sidebar.CollapsedStyleClassName);

        this.element.setAttribute("role", role || "group");
        if (label)
            this.element.setAttribute("aria-label", label);

        if (hasNavigationBar) {
            this.element.classList.add("has-navigation-bar");

            const navigationBarElement = null;
            this._navigationBar = new WI.NavigationBar(navigationBarElement, {role: "tablist"});
            this._navigationBar.addEventListener(WI.NavigationBar.Event.NavigationItemSelected, this._navigationItemSelected, this);
            this.addSubview(this._navigationBar);
        }

        this._resizer = new WI.Resizer(WI.Resizer.RuleOrientation.Vertical, this);
        this.element.insertBefore(this._resizer.element, this.element.firstChild);

        this._sidebarPanels = [];

        if (sidebarPanels) {
            for (let sidebarPanel of sidebarPanels)
                this.addSidebarPanel(sidebarPanel);
        }
    }

    // Public

    addSidebarPanel(sidebarPanel)
    {
        this.insertSidebarPanel(sidebarPanel, this._sidebarPanels.length);
    }

    insertSidebarPanel(sidebarPanel, index)
    {
        console.assert(sidebarPanel instanceof WI.SidebarPanel);
        if (!(sidebarPanel instanceof WI.SidebarPanel))
            return;

        if (sidebarPanel.parentSidebar && sidebarPanel.parentSidebar !== this) {
            console.assert(false, "Failed to insert sidebar panel", sidebarPanel);
            return;
        }

        console.assert(index >= 0 && index <= this._sidebarPanels.length);
        this._sidebarPanels.splice(index, 0, sidebarPanel);

        if (this._navigationBar) {
            console.assert(sidebarPanel.navigationItem);
            this._navigationBar.insertNavigationItem(sidebarPanel.navigationItem, index);
        }
    }

    removeSidebarPanel(sidebarPanelOrIdentifierOrIndex)
    {
        let sidebarPanel = this.findSidebarPanel(sidebarPanelOrIdentifierOrIndex);
        if (!sidebarPanel)
            return;

        if (sidebarPanel.visible)
            sidebarPanel.hidden();

        sidebarPanel.selected = false;

        this._sidebarPanels.remove(sidebarPanel);

        if (this._navigationBar) {
            console.assert(sidebarPanel.navigationItem);
            this._navigationBar.removeNavigationItem(sidebarPanel.navigationItem);
        }
    }

    get selectedSidebarPanel()
    {
        return this._selectedSidebarPanel || null;
    }

    set selectedSidebarPanel(sidebarPanelOrIdentifierOrIndex)
    {
        var sidebarPanel = this.findSidebarPanel(sidebarPanelOrIdentifierOrIndex);
        if (this._selectedSidebarPanel === sidebarPanel)
            return;

        if (this._selectedSidebarPanel) {
            this._selectedSidebarPanel.hidden();
            this._selectedSidebarPanel.selected = false;
            this.removeSubview(this._selectedSidebarPanel);
        }

        this._selectedSidebarPanel = sidebarPanel || null;

        if (this._navigationBar)
            this._navigationBar.selectedNavigationItem = sidebarPanel ? sidebarPanel.navigationItem : null;

        if (this._selectedSidebarPanel) {
            this.addSubview(this._selectedSidebarPanel);
            this._selectedSidebarPanel.selected = true;
            if (!this.collapsed)
                this._selectedSidebarPanel.shown();
        }

        this.dispatchEventToListeners(WI.Sidebar.Event.SidebarPanelSelected);
    }

    get minimumWidth()
    {
        let minimumWidth = WI.Sidebar.AbsoluteMinimumWidth;
        if (this._navigationBar)
            minimumWidth = Math.max(minimumWidth, this._navigationBar.minimumWidth);
        if (this._selectedSidebarPanel)
            minimumWidth = Math.max(minimumWidth, this._selectedSidebarPanel.minimumWidth);
        return minimumWidth;
    }

    get maximumWidth()
    {
        return WI.getMaximumSidebarWidth(this);
    }

    get width()
    {
        return this.element.offsetWidth;
    }

    set width(newWidth)
    {
        if (newWidth === this.width)
            return;

        this._recalculateWidth(newWidth);
    }

    get collapsed()
    {
        return this._collapsed;
    }

    set collapsed(flag)
    {
        if (flag === this._collapsed)
            return;

        this._collapsed = flag || false;
        this.element.classList.toggle(WI.Sidebar.CollapsedStyleClassName);

        if (!this._collapsed && this._navigationBar)
            this._navigationBar.needsLayout();

        if (this._selectedSidebarPanel) {
            if (this._selectedSidebarPanel.visible)
                this._selectedSidebarPanel.shown();
            else
                this._selectedSidebarPanel.hidden();
        }

        this.dispatchEventToListeners(WI.Sidebar.Event.CollapsedStateDidChange);
        this.dispatchEventToListeners(WI.Sidebar.Event.WidthDidChange);
    }

    get sidebarPanels()
    {
        return this._sidebarPanels;
    }

    get side()
    {
        return this._side;
    }

    findSidebarPanel(sidebarPanelOrIdentifierOrIndex)
    {
        var sidebarPanel = null;

        if (sidebarPanelOrIdentifierOrIndex instanceof WI.SidebarPanel) {
            if (this._sidebarPanels.includes(sidebarPanelOrIdentifierOrIndex))
                sidebarPanel = sidebarPanelOrIdentifierOrIndex;
        } else if (typeof sidebarPanelOrIdentifierOrIndex === "number") {
            sidebarPanel = this._sidebarPanels[sidebarPanelOrIdentifierOrIndex];
        } else if (typeof sidebarPanelOrIdentifierOrIndex === "string") {
            for (var i = 0; i < this._sidebarPanels.length; ++i) {
                if (this._sidebarPanels[i].identifier === sidebarPanelOrIdentifierOrIndex) {
                    sidebarPanel = this._sidebarPanels[i];
                    break;
                }
            }
        }

        return sidebarPanel;
    }

    // Protected

    resizerDragStarted(resizer)
    {
        this._widthBeforeResize = this.width;
    }

    resizerDragging(resizer, positionDelta)
    {
        if (this._side === WI.Sidebar.Sides.Left)
            positionDelta *= -1;

        if (WI.resolvedLayoutDirection() === WI.LayoutDirection.RTL)
            positionDelta *= -1;

        var newWidth = positionDelta + this._widthBeforeResize;
        this.width = newWidth;
        this.collapsed = newWidth < (this.minimumWidth / 2);
    }

    resizerDragEnded(resizer)
    {
        if (this._widthBeforeResize === this.width)
            return;

        if (!this.collapsed && this._navigationBar)
            this._navigationBar.sizeDidChange();

        if (!this.collapsed && this._selectedSidebarPanel)
            this._selectedSidebarPanel.sizeDidChange();
    }

    // Private

    _recalculateWidth(newWidth = this.width)
    {
        // Need to add 1 because of the 1px border-right.
        newWidth = Math.ceil(Number.constrain(newWidth, this.minimumWidth + 1, this.maximumWidth));
        this.element.style.width = `${newWidth}px`;

        if (this.collapsed)
            return;

        if (this._navigationBar)
            this._navigationBar.updateLayout(WI.View.LayoutReason.Resize);

        if (this._selectedSidebarPanel)
            this._selectedSidebarPanel.updateLayout(WI.View.LayoutReason.Resize);

        this.dispatchEventToListeners(WI.Sidebar.Event.WidthDidChange, {newWidth});
    }

    _navigationItemSelected(event)
    {
        this.selectedSidebarPanel = event.target.selectedNavigationItem ? event.target.selectedNavigationItem.identifier : null;
    }
};

WI.Sidebar.CollapsedStyleClassName = "collapsed";
WI.Sidebar.AbsoluteMinimumWidth = 200;

WI.Sidebar.Sides = {
    Right: "right",
    Left: "left"
};

WI.Sidebar.Event = {
    SidebarPanelSelected: "sidebar-panel-selected",
    CollapsedStateDidChange: "sidebar-collapsed-state-did-change",
    WidthDidChange: "sidebar-width-did-change",
};
