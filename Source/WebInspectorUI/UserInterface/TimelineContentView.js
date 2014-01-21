/*
 * Copyright (C) 2013 Apple Inc. All rights reserved.
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

WebInspector.TimelineContentView = function(recording)
{
    WebInspector.ContentView.call(this, recording);

    this.element.classList.add(WebInspector.TimelineContentView.StyleClassName);
};

WebInspector.TimelineContentView.StyleClassName = "timeline";
WebInspector.TimelineContentView.ViewContainerStyleClassName = "view-container";

WebInspector.TimelineContentView.prototype = {
    constructor: WebInspector.TimelineContentView,
    __proto__: WebInspector.ContentView.prototype,

    // Public

    showOverviewTimelineView: function()
    {
        // FIXME: Implement.
    },

    showTimelineView: function(identifier)
    {
        // FIXME: Implement.
    },

    get allowedNavigationSidebarPanels()
    {
        return ["timeline"];
    },

    get supportsSplitContentBrowser()
    {
        // The layout of the overview and split content browser don't work well.
        return false;
    }
};
