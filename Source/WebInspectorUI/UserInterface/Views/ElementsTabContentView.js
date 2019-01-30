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

WI.ElementsTabContentView = class ElementsTabContentView extends WI.ContentBrowserTabContentView
{
    constructor(identifier)
    {
        let tabBarItem = WI.GeneralTabBarItem.fromTabInfo(WI.ElementsTabContentView.tabInfo());

        let detailsSidebarPanelConstructors = [WI.RulesStyleDetailsSidebarPanel, WI.ComputedStyleDetailsSidebarPanel, WI.ChangesDetailsSidebarPanel, WI.DOMNodeDetailsSidebarPanel];
        if (window.LayerTreeAgent)
            detailsSidebarPanelConstructors.push(WI.LayerTreeDetailsSidebarPanel);

        super(identifier || "elements", "elements", tabBarItem, null, detailsSidebarPanelConstructors, true);

        WI.networkManager.addEventListener(WI.NetworkManager.Event.MainFrameDidChange, this._mainFrameDidChange, this);
        WI.Frame.addEventListener(WI.Frame.Event.MainResourceDidChange, this._mainResourceDidChange, this);
    }

    static tabInfo()
    {
        return {
            image: "Images/Elements.svg",
            title: WI.UIString("Elements"),
        };
    }

    static isTabAllowed()
    {
        return !!window.DOMAgent;
    }

    // Public

    get type()
    {
        return WI.ElementsTabContentView.Type;
    }

    get supportsSplitContentBrowser()
    {
        return true;
    }

    canShowRepresentedObject(representedObject)
    {
        return representedObject instanceof WI.DOMTree;
    }

    showRepresentedObject(representedObject, cookie)
    {
        if (!this.contentBrowser.currentContentView)
            this._showDOMTreeContentView();

        if (!cookie || !cookie.nodeToSelect)
            return;

        let domTreeContentView = this.contentBrowser.currentContentView;
        console.assert(domTreeContentView instanceof WI.DOMTreeContentView, "Unexpected DOMTreeContentView representedObject.", domTreeContentView);

        domTreeContentView.selectAndRevealDOMNode(cookie.nodeToSelect);

        // Because nodeToSelect is ephemeral, we don't want to keep
        // it around in the back-forward history entries.
        cookie.nodeToSelect = undefined;
    }

    shown()
    {
        super.shown();

        if (!this.contentBrowser.currentContentView)
            this._showDOMTreeContentView();
    }

    closed()
    {
        super.closed();

        WI.networkManager.removeEventListener(null, null, this);
        WI.Frame.removeEventListener(null, null, this);
    }

    // Private

    _showDOMTreeContentView()
    {
        this.contentBrowser.contentViewContainer.closeAllContentViews();

        var mainFrame = WI.networkManager.mainFrame;
        if (mainFrame)
            this.contentBrowser.showContentViewForRepresentedObject(mainFrame.domTree);
    }

    _mainFrameDidChange(event)
    {
        this._showDOMTreeContentView();
    }

    _mainResourceDidChange(event)
    {
        if (!event.target.isMainFrame())
            return;

        this._showDOMTreeContentView();
    }
};

WI.ElementsTabContentView.Type = "elements";
