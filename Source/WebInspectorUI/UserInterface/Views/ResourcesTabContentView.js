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

WebInspector.ResourcesTabContentView = class ResourcesTabContentView extends WebInspector.ContentBrowserTabContentView
{
    constructor(identifier)
    {
        let {image, title} = WebInspector.ResourcesTabContentView.tabInfo();
        let tabBarItem = new WebInspector.GeneralTabBarItem(image, title);
        let detailsSidebarPanels = [WebInspector.resourceDetailsSidebarPanel, WebInspector.probeDetailsSidebarPanel];

        // FIXME: Until ContentFlows are moved to the Elements tab, these details sidebar panels need to be included.
        detailsSidebarPanels = detailsSidebarPanels.concat([WebInspector.domNodeDetailsSidebarPanel, WebInspector.cssStyleDetailsSidebarPanel]);
        if (WebInspector.layerTreeDetailsSidebarPanel)
            detailsSidebarPanels.push(WebInspector.layerTreeDetailsSidebarPanel);

        super(identifier || "resources", "resources", tabBarItem, WebInspector.ResourceSidebarPanel, detailsSidebarPanels);
    }

    static tabInfo()
    {
        return {
            image: "Images/Resources.svg",
            title: WebInspector.UIString("Resources"),
        };
    }

    // Public

    get type()
    {
        return WebInspector.ResourcesTabContentView.Type;
    }

    get supportsSplitContentBrowser()
    {
        return true;
    }

    canShowRepresentedObject(representedObject)
    {
        return representedObject instanceof WebInspector.Frame
            || representedObject instanceof WebInspector.Resource
            || representedObject instanceof WebInspector.Script
            || representedObject instanceof WebInspector.ContentFlow
            || representedObject instanceof WebInspector.Collection;
    }
};

WebInspector.ResourcesTabContentView.Type = "resources";
