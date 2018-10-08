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

WI.NetworkResourceDetailView = class NetworkResourceDetailView extends WI.View
{
    constructor(resource, delegate)
    {
        super();

        console.assert(resource instanceof WI.Resource);

        this._resource = resource;
        this._delegate = delegate || null;

        this.element.classList.add("network-resource-detail");

        this._contentBrowser = null;
        this._resourceContentView = null;
        this._headersContentView = null;
        this._cookiesContentView = null;
        this._sizesContentView = null;
        this._timingContentView = null;

        this._contentViewCookie = null;
    }

    // Public

    get resource() { return this._resource; }

    shown()
    {
        if (!this._contentBrowser)
            return;

        this._showPreferredContentView();

        if (this._contentViewCookie) {
            if ("lineNumber" in this._contentViewCookie && "columnNumber" in this._contentViewCookie)
                this._contentBrowser.navigationBar.selectedNavigationItem = this._previewNavigationItem;

            this._contentBrowser.showContentView(this._contentBrowser.currentContentView, this._contentViewCookie);
            this._contentViewCookie = null;
        }

        this._contentBrowser.shown();
    }

    hidden()
    {
        this._contentBrowser.hidden();
    }

    dispose()
    {
        this._delegate = null;

        this._contentBrowser.contentViewContainer.closeAllContentViews();
    }

    willShowWithCookie(cookie)
    {
        this._contentViewCookie = cookie;
    }

    // ResourceHeadersContentView delegate

    headersContentViewGoToRequestData(headersContentView)
    {
        this._contentBrowser.navigationBar.selectedNavigationItem = this._previewNavigationItem;

        this._resourceContentView.showRequest();
    }

    // ResourceSizesContentView delegate

    sizesContentViewGoToHeaders(metricsContentView)
    {
        this._contentBrowser.navigationBar.selectedNavigationItem = this._headersNavigationItem;
    }

    sizesContentViewGoToRequestBody(metricsContentView)
    {
        this._contentBrowser.navigationBar.selectedNavigationItem = this._previewNavigationItem;

        this._resourceContentView.showRequest();
    }

    sizesContentViewGoToResponseBody(metricsContentView)
    {
        this._contentBrowser.navigationBar.selectedNavigationItem = this._previewNavigationItem;

        this._resourceContentView.showResponse();
    }

    // Protected

    initialLayout()
    {
        let closeNavigationItem = new WI.ButtonNavigationItem("close", WI.UIString("Close detail view"), "Images/CloseLarge.svg", 16, 16);
        closeNavigationItem.addEventListener(WI.ButtonNavigationItem.Event.Clicked, this._handleCloseButton.bind(this));
        closeNavigationItem.visibilityPriority = WI.NavigationItem.VisibilityPriority.High;

        let contentViewNavigationItemsGroup = new WI.GroupNavigationItem;
        let contentViewNavigationItemsFlexItem = new WI.FlexibleSpaceNavigationItem(contentViewNavigationItemsGroup, WI.FlexibleSpaceNavigationItem.Align.End);
        contentViewNavigationItemsFlexItem.visibilityPriority = WI.NavigationItem.VisibilityPriority.Low;

        const disableBackForward = true;
        const disableFindBanner = false;
        this._contentBrowser = new WI.ContentBrowser(null, this, disableBackForward, disableFindBanner, contentViewNavigationItemsFlexItem, contentViewNavigationItemsGroup);

        this._previewNavigationItem = new WI.RadioButtonNavigationItem("preview", WI.UIString("Preview"));
        this._headersNavigationItem = new WI.RadioButtonNavigationItem("headers", WI.UIString("Headers"));
        this._cookiesNavigationItem = new WI.RadioButtonNavigationItem("cookies", WI.UIString("Cookies"));
        this._sizesNavigationItem = new WI.RadioButtonNavigationItem("sizes", WI.UIString("Sizes"));
        this._timingNavigationItem = new WI.RadioButtonNavigationItem("timing", WI.UIString("Timing"));

        // Insert all of our custom navigation items at the start of the ContentBrowser's NavigationBar.
        let index = 0;
        this._contentBrowser.navigationBar.insertNavigationItem(closeNavigationItem, index++);
        this._contentBrowser.navigationBar.insertNavigationItem(new WI.FlexibleSpaceNavigationItem, index++);
        this._contentBrowser.navigationBar.insertNavigationItem(this._previewNavigationItem, index++);
        this._contentBrowser.navigationBar.insertNavigationItem(this._headersNavigationItem, index++);
        this._contentBrowser.navigationBar.insertNavigationItem(this._cookiesNavigationItem, index++);
        this._contentBrowser.navigationBar.insertNavigationItem(this._sizesNavigationItem, index++);
        this._contentBrowser.navigationBar.insertNavigationItem(this._timingNavigationItem, index++);
        this._contentBrowser.navigationBar.addEventListener(WI.NavigationBar.Event.NavigationItemSelected, this._navigationItemSelected, this);

        this.addSubview(this._contentBrowser);

        this._showPreferredContentView();
    }

    // Private

    _showPreferredContentView()
    {
        // Restore the preferred navigation item.
        let firstNavigationItem = null;
        let defaultIdentifier = WI.settings.selectedNetworkDetailContentViewIdentifier.value;
        for (let navigationItem of this._contentBrowser.navigationBar.navigationItems) {
            if (!(navigationItem instanceof WI.RadioButtonNavigationItem))
                continue;

            if (navigationItem !== this._previewNavigationItem
                && navigationItem !== this._headersNavigationItem
                && navigationItem !== this._cookiesNavigationItem
                && navigationItem !== this._sizesNavigationItem
                && navigationItem !== this._timingNavigationItem)
                continue;

            if (!firstNavigationItem)
                firstNavigationItem = navigationItem;

            if (navigationItem.identifier === defaultIdentifier) {
                this._contentBrowser.navigationBar.selectedNavigationItem = navigationItem;
                return;
            }
        }

        console.assert(firstNavigationItem, "Should have found at least one navigation item above");
        this._contentBrowser.navigationBar.selectedNavigationItem = firstNavigationItem;
    }

    _showContentViewForNavigationItem(navigationItem)
    {
        let identifier = navigationItem.identifier;
        if (this._contentViewCookie && "lineNumber" in this._contentViewCookie && "columnNumber" in this._contentViewCookie)
            identifier = this._previewNavigationItem.identifier;

        switch (identifier) {
        case "preview":
            if (!this._resourceContentView)
                this._resourceContentView = this._contentBrowser.showContentViewForRepresentedObject(this._resource);
            this._contentBrowser.showContentView(this._resourceContentView, this._contentViewCookie);
            break;
        case "headers":
            if (!this._headersContentView)
                this._headersContentView = new WI.ResourceHeadersContentView(this._resource, this);
            this._contentBrowser.showContentView(this._headersContentView, this._contentViewCookie);
            break;
        case "cookies":
            if (!this._cookiesContentView)
                this._cookiesContentView = new WI.ResourceCookiesContentView(this._resource);
            this._contentBrowser.showContentView(this._cookiesContentView, this._contentViewCookie);
            break;
        case "sizes":
            if (!this._sizesContentView)
                this._sizesContentView = new WI.ResourceSizesContentView(this._resource, this);
            this._contentBrowser.showContentView(this._sizesContentView, this._contentViewCookie);
            break;
        case "timing":
            if (!this._timingContentView)
                this._timingContentView = new WI.ResourceTimingContentView(this._resource);
            this._contentBrowser.showContentView(this._timingContentView, this._contentViewCookie);
            break;
        }

        this._contentViewCookie = null;
    }

    _navigationItemSelected(event)
    {
        let navigationItem = event.target.selectedNavigationItem;
        if (!(navigationItem instanceof WI.RadioButtonNavigationItem))
            return;

        this._showContentViewForNavigationItem(navigationItem);

        console.assert(navigationItem.identifier);
        WI.settings.selectedNetworkDetailContentViewIdentifier.value = navigationItem.identifier;
    }

    _handleCloseButton(event)
    {
        if (this._delegate && this._delegate.networkResourceDetailViewClose)
            this._delegate.networkResourceDetailViewClose(this);
    }
};
