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

WI.DebuggerTabContentView = class DebuggerTabContentView extends WI.ContentBrowserTabContentView
{
    constructor(identifier)
    {
        let tabBarItem = WI.GeneralTabBarItem.fromTabInfo(WI.DebuggerTabContentView.tabInfo());
        let detailsSidebarPanelConstructors = [WI.ScopeChainDetailsSidebarPanel, WI.ResourceDetailsSidebarPanel, WI.ProbeDetailsSidebarPanel];

        super(identifier || "debugger", "debugger", tabBarItem, WI.DebuggerSidebarPanel, detailsSidebarPanelConstructors);
    }

    static tabInfo()
    {
        return {
            image: "Images/Debugger.svg",
            title: WI.UIString("Debugger"),
        };
    }

    static isTabAllowed()
    {
        return !WI.settings.experimentalEnableSourcesTab.value;
    }

    // Public

    get type()
    {
        return WI.DebuggerTabContentView.Type;
    }

    get supportsSplitContentBrowser()
    {
        return true;
    }

    canShowRepresentedObject(representedObject)
    {
        if (representedObject instanceof WI.Script)
            return true;

        if (!(representedObject instanceof WI.Resource))
            return false;

        return representedObject.type === WI.Resource.Type.Document || representedObject.type === WI.Resource.Type.Script;
    }

    showDetailsSidebarPanels()
    {
        super.showDetailsSidebarPanels();

        if (!this._showScopeChainDetailsSidebarPanel)
            return;

        let scopeChainDetailsSidebarPanel = this.detailsSidebarPanels.find((item) => item instanceof WI.ScopeChainDetailsSidebarPanel);
        if (!scopeChainDetailsSidebarPanel)
            return;

        WI.detailsSidebar.selectedSidebarPanel = scopeChainDetailsSidebarPanel;
        WI.detailsSidebar.collapsed = false;

        this._showScopeChainDetailsSidebarPanel = false;
    }

    showScopeChainDetailsSidebarPanel()
    {
        this._showScopeChainDetailsSidebarPanel = true;
    }

    revealAndSelectBreakpoint(breakpoint)
    {
        console.assert(breakpoint instanceof WI.Breakpoint);

        var treeElement = this.navigationSidebarPanel.treeElementForRepresentedObject(breakpoint);
        if (treeElement)
            treeElement.revealAndSelect();
    }
};

WI.DebuggerTabContentView.Type = "debugger";
