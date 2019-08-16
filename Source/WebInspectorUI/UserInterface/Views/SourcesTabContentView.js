/*
 * Copyright (C) 2018 Apple Inc. All rights reserved.
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

WI.SourcesTabContentView = class SourcesTabContentView extends WI.ContentBrowserTabContentView
{
    constructor()
    {
        let tabBarItem = WI.GeneralTabBarItem.fromTabInfo(WI.SourcesTabContentView.tabInfo());
        const detailsSidebarPanelConstructors = [WI.ResourceDetailsSidebarPanel, WI.ScopeChainDetailsSidebarPanel, WI.ProbeDetailsSidebarPanel];
        super("sources", ["sources"], tabBarItem, WI.SourcesNavigationSidebarPanel, detailsSidebarPanelConstructors);

        this._showScopeChainDetailsSidebarPanel = false;
    }

    static tabInfo()
    {
        return {
            image: "Images/Sources.svg",
            title: WI.UIString("Sources"),
        };
    }

    static isTabAllowed()
    {
        return !!WI.settings.experimentalEnableSourcesTab.value;
    }

    // Public

    get type()
    {
        return WI.SourcesTabContentView.Type;
    }

    get supportsSplitContentBrowser()
    {
        return true;
    }

    canShowRepresentedObject(representedObject)
    {
        return representedObject instanceof WI.Frame
            || representedObject instanceof WI.FrameCollection
            || representedObject instanceof WI.Resource
            || representedObject instanceof WI.ResourceCollection
            || representedObject instanceof WI.Script
            || representedObject instanceof WI.ScriptCollection
            || representedObject instanceof WI.CSSStyleSheet
            || representedObject instanceof WI.CSSStyleSheetCollection
            || super.canShowRepresentedObject(representedObject);
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

        let treeElement = this.navigationSidebarPanel.treeElementForRepresentedObject(breakpoint);
        if (treeElement)
            treeElement.revealAndSelect();
    }
};

WI.SourcesTabContentView.Type = "sources";
