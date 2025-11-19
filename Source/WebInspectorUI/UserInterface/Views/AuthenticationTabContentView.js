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


WI.AuthenticationTabContentView = class AuthenticationTabContentView extends WI.TabContentView
{
    constructor()
    {
        super(WI.AuthenticationTabContentView.tabInfo());

        this._authenticationTableContentView = new WI.AuthenticationTableContentView;

        this._contentBrowser = new WI.ContentBrowser(null, this, {hideBackForwardButtons: true, disableFindBanner: true});
        this._contentBrowser.showContentView(this._authenticationTableContentView);

        this.addSubview(this._contentBrowser);
    }

    // Static

    static tabInfo()
    {
        return {
            identifier: WI.AuthenticationTabContentView.Type,
            image: "Images/Key.svg",
            displayName: WI.UIString("Web Authentication", "Web Authentication Tab Name", "Name of Web Authentication Tab"),
        };
    }

    static isTabAllowed()
    {
        return InspectorBackend.hasDomain("Page");
    }

    // Public

    get contentBrowser() { return this._contentBrowser; }

    get type()
    {
        return WI.AuthenticationTabContentView.Type;
    }

    attached()
    {
        super.attached();
    }

    detached()
    {
        super.detached();
    }

    closed()
    {
        super.closed();
    }

    get supportsSplitContentBrowser()
    {
        return true;
    }

    get canHandleFindEvent()
    {
        return false;
    }
};

WI.AuthenticationTabContentView.Type = "authentication";
