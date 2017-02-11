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

WebInspector.ContentBrowserTabContentView = class ContentBrowserTabContentView extends WebInspector.TabContentView
{
    constructor(identifier, styleClassNames, tabBarItem, navigationSidebarPanelClass, detailsSidebarPanels, disableBackForward)
    {
        if (typeof styleClassNames === "string")
            styleClassNames = [styleClassNames];

        styleClassNames.push("content-browser");

        var contentBrowser = new WebInspector.ContentBrowser(null, null, disableBackForward);
        var navigationSidebarPanel = navigationSidebarPanelClass ? new navigationSidebarPanelClass(contentBrowser) : null;

        super(identifier, styleClassNames, tabBarItem, navigationSidebarPanel, detailsSidebarPanels);

        this._contentBrowser = contentBrowser;
        this._contentBrowser.delegate = this;

        this._lastSelectedDetailsSidebarPanelSetting = new WebInspector.Setting(identifier + "-last-selected-details-sidebar-panel", null);

        this._contentBrowser.addEventListener(WebInspector.ContentBrowser.Event.CurrentRepresentedObjectsDidChange, this.showDetailsSidebarPanels, this);
        this._contentBrowser.addEventListener(WebInspector.ContentBrowser.Event.CurrentContentViewDidChange, this._contentBrowserCurrentContentViewDidChange, this);

        // If any content views were shown during sidebar construction, contentBrowserTreeElementForRepresentedObject() would have returned null.
        // Explicitly update the path for the navigation bar to prevent it from showing up as blank.
        this._contentBrowser.updateHierarchicalPathForCurrentContentView();

        if (navigationSidebarPanel) {
            var showToolTip = WebInspector.UIString("Show the navigation sidebar (%s)").format(WebInspector.navigationSidebarKeyboardShortcut.displayName);
            var hideToolTip = WebInspector.UIString("Hide the navigation sidebar (%s)").format(WebInspector.navigationSidebarKeyboardShortcut.displayName);

            this._showNavigationSidebarItem = new WebInspector.ActivateButtonNavigationItem("toggle-navigation-sidebar", showToolTip, hideToolTip, "Images/ToggleLeftSidebar.svg", 16, 16);
            this._showNavigationSidebarItem.addEventListener(WebInspector.ButtonNavigationItem.Event.Clicked, WebInspector.toggleNavigationSidebar, WebInspector);
            this._showNavigationSidebarItem.activated = !WebInspector.navigationSidebar.collapsed;

            this._contentBrowser.navigationBar.insertNavigationItem(this._showNavigationSidebarItem, 0);
            this._contentBrowser.navigationBar.insertNavigationItem(new WebInspector.DividerNavigationItem, 1);

            navigationSidebarPanel.contentBrowser = this._contentBrowser;

            WebInspector.navigationSidebar.addEventListener(WebInspector.Sidebar.Event.CollapsedStateDidChange, this._navigationSidebarCollapsedStateDidChange, this);
        }

        if (detailsSidebarPanels && detailsSidebarPanels.length) {
            var showToolTip = WebInspector.UIString("Show the details sidebar (%s)").format(WebInspector.detailsSidebarKeyboardShortcut.displayName);
            var hideToolTip = WebInspector.UIString("Hide the details sidebar (%s)").format(WebInspector.detailsSidebarKeyboardShortcut.displayName);

            this._showDetailsSidebarItem = new WebInspector.ActivateButtonNavigationItem("toggle-details-sidebar", showToolTip, hideToolTip, "Images/ToggleRightSidebar.svg", 16, 16);
            this._showDetailsSidebarItem.addEventListener(WebInspector.ButtonNavigationItem.Event.Clicked, WebInspector.toggleDetailsSidebar, WebInspector);
            this._showDetailsSidebarItem.activated = !WebInspector.detailsSidebar.collapsed;
            this._showDetailsSidebarItem.enabled = false;

            this._contentBrowser.navigationBar.addNavigationItem(new WebInspector.DividerNavigationItem);
            this._contentBrowser.navigationBar.addNavigationItem(this._showDetailsSidebarItem);

            WebInspector.detailsSidebar.addEventListener(WebInspector.Sidebar.Event.CollapsedStateDidChange, this._detailsSidebarCollapsedStateDidChange, this);
            WebInspector.detailsSidebar.addEventListener(WebInspector.Sidebar.Event.SidebarPanelSelected, this._detailsSidebarPanelSelected, this);
        }

        this.addSubview(this._contentBrowser);
    }

    // Public

    get contentBrowser()
    {
        return this._contentBrowser;
    }

    shown()
    {
        super.shown();

        this._contentBrowser.shown();

        if (this.navigationSidebarPanel && !this._contentBrowser.currentContentView)
            this.navigationSidebarPanel.showDefaultContentView();
    }

    hidden()
    {
        super.hidden();

        this._contentBrowser.hidden();
    }

    closed()
    {
        super.closed();

        WebInspector.navigationSidebar.removeEventListener(null, null, this);
        WebInspector.detailsSidebar.removeEventListener(null, null, this);

        if (this.navigationSidebarPanel && typeof this.navigationSidebarPanel.closed === "function")
            this.navigationSidebarPanel.closed();

        this._contentBrowser.contentViewContainer.closeAllContentViews();
    }

    get managesDetailsSidebarPanels()
    {
        return true;
    }

    showDetailsSidebarPanels()
    {
        if (!this.visible)
            return;

        var currentRepresentedObjects = this._contentBrowser.currentRepresentedObjects;
        var currentSidebarPanels = WebInspector.detailsSidebar.sidebarPanels;
        var wasSidebarEmpty = !currentSidebarPanels.length;

        // Ignore any changes to the selected sidebar panel during this function so only user initiated
        // changes are recorded in _lastSelectedDetailsSidebarPanelSetting.
        this._ignoreDetailsSidebarPanelSelectedEvent = true;
        this._ignoreDetailsSidebarPanelCollapsedEvent = true;

        for (var i = 0; i < this.detailsSidebarPanels.length; ++i) {
            var sidebarPanel = this.detailsSidebarPanels[i];
            if (sidebarPanel.inspect(currentRepresentedObjects)) {
                if (currentSidebarPanels.includes(sidebarPanel)) {
                    // Already showing the panel.
                    continue;
                }

                // The sidebar panel was not previously showing, so add the panel.
                WebInspector.detailsSidebar.addSidebarPanel(sidebarPanel);

                if (this._lastSelectedDetailsSidebarPanelSetting.value === sidebarPanel.identifier) {
                    // Restore the sidebar panel selection if this sidebar panel was the last one selected by the user.
                    WebInspector.detailsSidebar.selectedSidebarPanel = sidebarPanel;
                }
            } else {
                // The sidebar panel can't inspect the current represented objects, so remove the panel and hide the toolbar item.
                WebInspector.detailsSidebar.removeSidebarPanel(sidebarPanel);
            }
        }

        if (!WebInspector.detailsSidebar.selectedSidebarPanel && currentSidebarPanels.length)
            WebInspector.detailsSidebar.selectedSidebarPanel = currentSidebarPanels[0];

        if (!WebInspector.detailsSidebar.sidebarPanels.length)
            WebInspector.detailsSidebar.collapsed = true;
        else if (wasSidebarEmpty)
            WebInspector.detailsSidebar.collapsed = this.detailsSidebarCollapsedSetting.value;

        this._ignoreDetailsSidebarPanelCollapsedEvent = false;
        this._ignoreDetailsSidebarPanelSelectedEvent = false;

        if (!this.detailsSidebarPanels.length)
            return;

        this._showDetailsSidebarItem.enabled = WebInspector.detailsSidebar.sidebarPanels.length;
    }

    showRepresentedObject(representedObject, cookie)
    {
        if (this.navigationSidebarPanel)
            this.navigationSidebarPanel.cancelRestoringState();
        this.contentBrowser.showContentViewForRepresentedObject(representedObject, cookie);
    }

    // ContentBrowser Delegate

    contentBrowserTreeElementForRepresentedObject(contentBrowser, representedObject)
    {
        return this.treeElementForRepresentedObject(representedObject);
    }

    // Protected

    treeElementForRepresentedObject(representedObject)
    {
        // Can be overriden by subclasses.

        if (!this.navigationSidebarPanel)
            return null;

        return this.navigationSidebarPanel.treeElementForRepresentedObject(representedObject);
     }

    // Private

    _navigationSidebarCollapsedStateDidChange(event)
    {
        this._showNavigationSidebarItem.activated = !WebInspector.navigationSidebar.collapsed;
    }

    _detailsSidebarCollapsedStateDidChange(event)
    {
        if (!this.visible)
            return;

        this._showDetailsSidebarItem.activated = !WebInspector.detailsSidebar.collapsed;
        this._showDetailsSidebarItem.enabled = WebInspector.detailsSidebar.sidebarPanels.length;

        if (this._ignoreDetailsSidebarPanelCollapsedEvent)
            return;

        this.detailsSidebarCollapsedSetting.value = WebInspector.detailsSidebar.collapsed;
    }

    _detailsSidebarPanelSelected(event)
    {
        if (!this.visible)
            return;

        this._showDetailsSidebarItem.activated = !WebInspector.detailsSidebar.collapsed;
        this._showDetailsSidebarItem.enabled = WebInspector.detailsSidebar.sidebarPanels.length;

        if (!WebInspector.detailsSidebar.selectedSidebarPanel || this._ignoreDetailsSidebarPanelSelectedEvent)
            return;

        this._lastSelectedDetailsSidebarPanelSetting.value = WebInspector.detailsSidebar.selectedSidebarPanel.identifier;
    }

    _contentBrowserCurrentContentViewDidChange(event)
    {
        let currentContentView = this._contentBrowser.currentContentView;
        if (!currentContentView)
            return;

        this._revealAndSelectRepresentedObject(currentContentView.representedObject);
    }

    _revealAndSelectRepresentedObject(representedObject)
    {
        if (this.navigationSidebarPanel) {
            // If a tree outline is processing a selection currently then we can assume the selection does not
            // need to be changed. This is needed to allow breakpoint and call frame tree elements to be selected
            // without jumping back to selecting the resource tree element.
            for (let contentTreeOutline of this.navigationSidebarPanel.contentTreeOutlines) {
                if (contentTreeOutline.processingSelectionChange)
                    return;
            }
        }

        let treeElement = this.treeElementForRepresentedObject(representedObject);

        if (treeElement)
            treeElement.revealAndSelect(true, false, true, true);
        else if (this.navigationSidebarPanel && this.navigationSidebarPanel.contentTreeOutline.selectedTreeElement)
            this.navigationSidebarPanel.contentTreeOutline.selectedTreeElement.deselect(true);
    }
};
