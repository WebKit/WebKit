/*
 * Copyright (C) 2020 Apple Inc. All rights reserved.
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

WI.WebInspectorExtensionTabContentView = class WebInspectorExtensionTabContentView extends WI.TabContentView
{
    constructor(extension, extensionTabID, tabLabel, iconURL, sourceURL)
    {
        let tabInfo = {
            identifier: WI.WebInspectorExtensionTabContentView.Type,
            image: iconURL,
            displayName: tabLabel,
            title: tabLabel,
        };
        super(tabInfo);

        this._extension = extension;
        this._extensionTabID = extensionTabID;
        this._tabInfo = tabInfo;
        this._sourceURL = sourceURL;
    }

    // Public

    get extensionTabID() { return this._extensionTabID; }

    get type()
    {
        return WI.WebInspectorExtensionTabContentView.Type;
    }

    get supportsSplitContentBrowser()
    {
        return true;
    }

    tabInfo()
    {
        return this._tabInfo;
    }

    static shouldSaveTab() { return false; }

    // Protected

    initialLayout()
    {
        super.initialLayout();

        let iframeElement = this.element.appendChild(document.createElement("iframe"));
        iframeElement.src = this._sourceURL;
    }
};

WI.WebInspectorExtensionTabContentView.Type = "web-inspector-extension";
