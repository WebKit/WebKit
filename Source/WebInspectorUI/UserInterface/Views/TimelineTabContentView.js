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

WebInspector.TimelineTabContentView = function(identifier)
{
    var tabBarItem = new WebInspector.TabBarItem("Images/Timeline.svg", WebInspector.UIString("Timelines"));
    var detailsSidebarPanels = [WebInspector.resourceDetailsSidebarPanel, WebInspector.probeDetailsSidebarPanel];

    WebInspector.ContentBrowserTabContentView.call(this, identifier || "timeline", "timeline", tabBarItem, WebInspector.TimelineSidebarPanel, detailsSidebarPanels);
};

WebInspector.TimelineTabContentView.prototype = {
    constructor: WebInspector.TimelineTabContentView,
    __proto__: WebInspector.ContentBrowserTabContentView.prototype,

    // Public

    get type()
    {
        return WebInspector.TimelineTabContentView.Type;
    },

    shown: function()
    {
        WebInspector.ContentBrowserTabContentView.prototype.shown.call(this);

        WebInspector.timelineManager.autoCaptureOnPageLoad = true;
    },

    hidden: function()
    {
        WebInspector.ContentBrowserTabContentView.prototype.hidden.call(this);

        WebInspector.timelineManager.autoCaptureOnPageLoad = false;
    },

    canShowRepresentedObject: function(representedObject)
    {
        if (representedObject instanceof WebInspector.TimelineRecording)
            return true;

        // Only support showing a resource or script if we have that represented object in the sidebar.
        if (representedObject instanceof WebInspector.Resource || representedObject instanceof WebInspector.Script)
            return !!this.navigationSidebarPanel.treeElementForRepresentedObject(representedObject);

        return false;
    },

    get supportsSplitContentBrowser()
    {
        return false;
    }
};

WebInspector.TimelineTabContentView.Type = "timeline";
