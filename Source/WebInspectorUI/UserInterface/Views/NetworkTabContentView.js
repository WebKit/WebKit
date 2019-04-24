/*
 * Copyright (C) 2017 Apple Inc. All rights reserved.
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

WI.NetworkTabContentView = class NetworkTabContentView extends WI.TabContentView
{
    constructor(identifier)
    {
        let tabBarItem = WI.GeneralTabBarItem.fromTabInfo(WI.NetworkTabContentView.tabInfo());

        super(identifier || "network", "network", tabBarItem);

        this._networkTableContentView = new WI.NetworkTableContentView;

        const disableBackForward = true;
        const disableFindBanner = true;
        this._contentBrowser = new WI.ContentBrowser(null, this, disableBackForward, disableFindBanner);
        this._contentBrowser.showContentView(this._networkTableContentView);

        let filterNavigationItems = this._networkTableContentView.filterNavigationItems;
        for (let i = 0; i < filterNavigationItems.length; ++i)
            this._contentBrowser.navigationBar.insertNavigationItem(filterNavigationItems[i], i);

        this.addSubview(this._contentBrowser);
    }

    // Static

    static tabInfo()
    {
        return {
            image: "Images/Network.svg",
            title: WI.UIString("Network"),
        };
    }

    static isTabAllowed()
    {
        return !!window.NetworkAgent;
    }

    // Protected

    shown()
    {
        super.shown();

        this._contentBrowser.shown();
    }

    hidden()
    {
        this._contentBrowser.hidden();

        super.hidden();
    }

    closed()
    {
        this._contentBrowser.contentViewContainer.closeAllContentViews();

        super.closed();
    }

    async handleFileDrop(files)
    {
        await WI.FileUtilities.readJSON(files, (result) => this._networkTableContentView.processHAR(result));
    }

    // Public

    get contentBrowser() { return this._contentBrowser; }

    get type()
    {
        return WI.NetworkTabContentView.Type;
    }

    canShowRepresentedObject(representedObject)
    {
        return representedObject instanceof WI.Resource;
    }

    showRepresentedObject(representedObject, cookie)
    {
        console.assert(this._contentBrowser.currentContentView === this._networkTableContentView);
        this._networkTableContentView.showRepresentedObject(representedObject, cookie);
    }

    get supportsSplitContentBrowser()
    {
        return true;
    }
};

WI.NetworkTabContentView.Type = "network";
