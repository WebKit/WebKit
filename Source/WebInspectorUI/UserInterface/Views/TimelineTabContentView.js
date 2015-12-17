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

WebInspector.TimelineTabContentView = class TimelineTabContentView extends WebInspector.ContentBrowserTabContentView
{
    constructor(identifier)
    {
        let {image, title} = WebInspector.TimelineTabContentView.tabInfo();
        let tabBarItem = new WebInspector.TabBarItem(image, title);
        let detailsSidebarPanels = [WebInspector.resourceDetailsSidebarPanel, WebInspector.probeDetailsSidebarPanel];

        super(identifier || "timeline", "timeline", tabBarItem, WebInspector.TimelineSidebarPanel, detailsSidebarPanels);
    }

    static tabInfo()
    {
        return {
            image: "Images/Timeline.svg",
            title: WebInspector.UIString("Timelines"),
        };
    }

    static isTabAllowed()
    {
        return !!window.TimelineAgent || !!window.ScriptProfilerAgent;
    }

    // Public

    get type()
    {
        return WebInspector.TimelineTabContentView.Type;
    }

    shown()
    {
        super.shown();

        WebInspector.timelineManager.autoCaptureOnPageLoad = true;
    }

    hidden()
    {
        super.hidden();

        WebInspector.timelineManager.autoCaptureOnPageLoad = false;
    }

    canShowRepresentedObject(representedObject)
    {
        if (representedObject instanceof WebInspector.TimelineRecording)
            return true;

        // Only support showing a resource or script if we have that represented object in the sidebar.
        if (representedObject instanceof WebInspector.Resource || representedObject instanceof WebInspector.Script)
            return !!this.navigationSidebarPanel.treeElementForRepresentedObject(representedObject);

        return false;
    }

    get supportsSplitContentBrowser()
    {
        return false;
    }
};

WebInspector.TimelineTabContentView.Type = "timeline";
