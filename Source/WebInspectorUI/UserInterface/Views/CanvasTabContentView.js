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

WI.CanvasTabContentView = class CanvasTabContentView extends WI.ContentBrowserTabContentView
{
    constructor(representedObject)
    {
        console.assert(!representedObject || representedObject instanceof WI.Canvas);

        let {image, title} = WI.CanvasTabContentView.tabInfo();
        let tabBarItem = new WI.GeneralTabBarItem(image, title);

        const navigationSidebarPanelConstructor = null;
        const detailsSidebarPanelConstructors = [WI.RecordingStateDetailsSidebarPanel, WI.RecordingTraceDetailsSidebarPanel, WI.CanvasDetailsSidebarPanel];
        const disableBackForward = true;
        super("canvas", ["canvas"], tabBarItem, navigationSidebarPanelConstructor, detailsSidebarPanelConstructors, disableBackForward);

        this._overviewPathComponent = new WI.HierarchicalPathComponent(WI.UIString("Canvas Overview"), "canvas-overview");
        this._overviewPathComponent.addEventListener(WI.HierarchicalPathComponent.Event.Clicked, this._overviewPathComponentClicked, this);

        this._canvasCollection = null;
        this._canvasOverviewContentView = null;
    }

    static tabInfo()
    {
        return {
            image: "Images/Canvas.svg",
            title: WI.UIString("Canvas"),
        };
    }

    static isTabAllowed()
    {
        // FIXME: remove experimental setting check once <https://webkit.org/b/175485> is complete.
        return !!window.CanvasAgent && WI.settings.experimentalEnableCanvasTab.value;
    }

    // Public

    get type()
    {
        return WI.CanvasTabContentView.Type;
    }

    get supportsSplitContentBrowser()
    {
        return true;
    }

    canShowRepresentedObject(representedObject)
    {
        return representedObject instanceof WI.CanvasCollection;
    }

    shown()
    {
        super.shown();

        if (this.contentBrowser.currentContentView)
            return;

        this._canvasOverviewContentView = new WI.CanvasOverviewContentView(this._canvasCollection);
        this.contentBrowser.showContentView(this._canvasOverviewContentView);
    }

    treeElementForRepresentedObject(representedObject)
    {
        if (!this._overviewTreeElement) {
            const title = WI.UIString("Canvas Overview");
            this._overviewTreeElement = new WI.GeneralTreeElement(["canvas-overview"], title, null, representedObject);
        }

        return this._overviewTreeElement;
    }

    restoreFromCookie(cookie)
    {
        // FIXME: implement once <https://webkit.org/b/177606> is complete.
    }

    saveStateToCookie(cookie)
    {
        // FIXME: implement once <https://webkit.org/b/177606> is complete.
    }

    // Protected

    attached()
    {
        super.attached();

        WI.canvasManager.addEventListener(WI.CanvasManager.Event.CanvasWasAdded, this._canvasAdded, this);
        WI.canvasManager.addEventListener(WI.CanvasManager.Event.CanvasWasRemoved, this._canvasRemoved, this);
        WI.Frame.addEventListener(WI.Frame.Event.MainResourceDidChange, this._mainResourceDidChange, this);

        this._canvasCollection = new WI.CanvasCollection(WI.canvasManager.canvases);
    }

    detached()
    {
        WI.canvasManager.removeEventListener(null, null, this);
        WI.Frame.removeEventListener(null, null, this);

        this._canvasCollection = null;

        super.detached();
    }

    // Private

    _canvasAdded(event)
    {
        let canvas = event.data.canvas;
        this._canvasCollection.add(canvas);
    }

    _canvasRemoved(event)
    {
        let canvas = event.data.canvas;
        this._canvasCollection.remove(canvas);
    }

    _overviewPathComponentClicked(event)
    {
        console.assert(this._canvasOverviewContentView);
        this.contentBrowser.showContentView(this._canvasOverviewContentView);
    }

    _mainResourceDidChange(event)
    {
        if (!event.target.isMainFrame())
            return;

        this._canvasCollection.clear();
    }
};

WI.CanvasTabContentView.Type = "canvas";
