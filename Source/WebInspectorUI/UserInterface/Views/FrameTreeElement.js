/*
 * Copyright (C) 2013, 2015 Apple Inc. All rights reserved.
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

WebInspector.FrameTreeElement = class FrameTreeElement extends WebInspector.ResourceTreeElement
{
    constructor(frame, representedObject)
    {
        console.assert(frame instanceof WebInspector.Frame);

        super(frame.mainResource, representedObject || frame);

        this._frame = frame;

        this._updateExpandedSetting();

        frame.addEventListener(WebInspector.Frame.Event.MainResourceDidChange, this._mainResourceDidChange, this);
        frame.addEventListener(WebInspector.Frame.Event.ResourceWasAdded, this._resourceWasAdded, this);
        frame.addEventListener(WebInspector.Frame.Event.ResourceWasRemoved, this._resourceWasRemoved, this);
        frame.addEventListener(WebInspector.Frame.Event.ChildFrameWasAdded, this._childFrameWasAdded, this);
        frame.addEventListener(WebInspector.Frame.Event.ChildFrameWasRemoved, this._childFrameWasRemoved, this);

        frame.domTree.addEventListener(WebInspector.DOMTree.Event.ContentFlowWasAdded, this._childContentFlowWasAdded, this);
        frame.domTree.addEventListener(WebInspector.DOMTree.Event.ContentFlowWasRemoved, this._childContentFlowWasRemoved, this);
        frame.domTree.addEventListener(WebInspector.DOMTree.Event.RootDOMNodeInvalidated, this._rootDOMNodeInvalidated, this);

        if (this._frame.isMainFrame())
            this._downloadingPage = false;

        this.shouldRefreshChildren = true;
        this.folderSettingsKey = this._frame.url.hash;

        this.registerFolderizeSettings("frames", WebInspector.UIString("Frames"),
            function(representedObject) { return representedObject instanceof WebInspector.Frame; },
            function() { return this.frame.childFrames.length; }.bind(this),
            WebInspector.FrameTreeElement
        );

        this.registerFolderizeSettings("flows", WebInspector.UIString("Flows"),
            function(representedObject) { return representedObject instanceof WebInspector.ContentFlow; },
            function() { return this.frame.domTree.flowsCount; }.bind(this),
            WebInspector.ContentFlowTreeElement
        );

        this.registerFolderizeSettings("canvases", WebInspector.UIString("Canvases"),
            function(representedObject) { return representedObject instanceof WebInspector.Canvas; },
            function() { return WebInspector.canvasManager.canvasesForFrame(this._frame).length; }.bind(this),
            WebInspector.CanvasTreeElement
        );

        function makeValidateCallback(resourceType) {
            return function(representedObject) {
                return representedObject instanceof WebInspector.Resource && representedObject.type === resourceType;
            };
        }

        function makeChildCountCallback(frame, resourceType) {
            return function() {
                return frame.resourcesWithType(resourceType).length;
            };
        }

        for (var key in WebInspector.Resource.Type) {
            var value = WebInspector.Resource.Type[key];
            var folderName = WebInspector.Resource.displayNameForType(value, true);
            this.registerFolderizeSettings(key, folderName,
                makeValidateCallback(value),
                makeChildCountCallback(this.frame, value),
                WebInspector.ResourceTreeElement
            );
        }

        this.updateParentStatus();
    }

    // Public

    get frame()
    {
        return this._frame;
    }

    descendantResourceTreeElementTypeDidChange(resourceTreeElement, oldType)
    {
        // Called by descendant ResourceTreeElements.

        // Add the tree element again, which will move it to the new location
        // based on sorting and possible folder changes.
        this._addTreeElement(resourceTreeElement);
    }

    descendantResourceTreeElementMainTitleDidChange(resourceTreeElement, oldMainTitle)
    {
        // Called by descendant ResourceTreeElements.

        // Add the tree element again, which will move it to the new location
        // based on sorting and possible folder changes.
        this._addTreeElement(resourceTreeElement);
    }

    // Overrides from SourceCodeTreeElement.

    updateSourceMapResources()
    {
        // Frames handle their own SourceMapResources.

        if (!this.treeOutline || !this.treeOutline.includeSourceMapResourceChildren)
            return;

        if (!this._frame)
            return;

        this.updateParentStatus();

        if (this.resource && this.resource.sourceMaps.length)
            this.shouldRefreshChildren = true;
    }

    onattach()
    {
        // Immediate superclasses are skipped, since Frames handle their own SourceMapResources.
        WebInspector.GeneralTreeElement.prototype.onattach.call(this);

        WebInspector.canvasManager.addEventListener(WebInspector.CanvasManager.Event.CanvasesAvailable, this._canvasesAvailable, this);
        WebInspector.canvasManager.addEventListener(WebInspector.CanvasManager.Event.CanvasWasAdded, this._canvasWasAdded, this);
        WebInspector.canvasManager.addEventListener(WebInspector.CanvasManager.Event.CanvasWasRemoved, this._canvasWasRemoved, this);

        if (this._frame.isMainFrame()) {
            WebInspector.notifications.addEventListener(WebInspector.Notification.PageArchiveStarted, this._pageArchiveStarted, this);
            WebInspector.notifications.addEventListener(WebInspector.Notification.PageArchiveEnded, this._pageArchiveEnded, this);
        }
    }

    ondetach()
    {
        WebInspector.ResourceTreeElement.prototype.ondetach.call(this);

        WebInspector.canvasManager.removeEventListener(WebInspector.CanvasManager.Event.CanvasesAvailable, this._canvasesAvailable, this);
        WebInspector.canvasManager.removeEventListener(WebInspector.CanvasManager.Event.CanvasWasAdded, this._canvasWasAdded, this);
        WebInspector.canvasManager.removeEventListener(WebInspector.CanvasManager.Event.CanvasWasRemoved, this._canvasWasRemoved, this);

        if (this._frame.isMainFrame()) {
            WebInspector.notifications.removeEventListener(WebInspector.Notification.PageArchiveStarted, this._pageArchiveStarted, this);
            WebInspector.notifications.removeEventListener(WebInspector.Notification.PageArchiveEnded, this._pageArchiveEnded, this);
        }
    }

    // Overrides from FolderizedTreeElement (Protected).

    compareChildTreeElements(a, b)
    {
        if (a === b)
            return 0;

        var aIsResource = a instanceof WebInspector.ResourceTreeElement;
        var bIsResource = b instanceof WebInspector.ResourceTreeElement;

        if (aIsResource && bIsResource)
            return WebInspector.ResourceTreeElement.compareResourceTreeElements(a, b);

        if (!aIsResource && !bIsResource) {
            // When both components are not resources then default to base class comparison.
            return super.compareChildTreeElements(a, b);
        }

        // Non-resources should appear before the resources.
        // FIXME: There should be a better way to group the elements by their type.
        return aIsResource ? 1 : -1;
    }

    // Called from ResourceTreeElement.

    updateStatusForMainFrame()
    {
        function loadedImages()
        {
            if (!this._reloadButton || !this._downloadButton)
                return;

            var fragment = document.createDocumentFragment("div");
            fragment.appendChild(this._downloadButton.element);
            fragment.appendChild(this._reloadButton.element);
            this.status = fragment;

            delete this._loadingMainFrameButtons;
        }

        if (this._reloadButton && this._downloadButton) {
            loadedImages.call(this);
            return;
        }

        if (!this._loadingMainFrameButtons) {
            this._loadingMainFrameButtons = true;

            var tooltip;
            if (WebInspector.debuggableType === WebInspector.DebuggableType.JavaScript)
                tooltip = WebInspector.UIString("Restart (%s)").format(WebInspector._reloadPageKeyboardShortcut.displayName);
            else
                tooltip = WebInspector.UIString("Reload page (%s)\nReload ignoring cache (%s)").format(WebInspector._reloadPageKeyboardShortcut.displayName, WebInspector._reloadPageIgnoringCacheKeyboardShortcut.displayName);

            wrappedSVGDocument(platformImagePath("Reload.svg"), null, tooltip, function(element) {
                this._reloadButton = new WebInspector.TreeElementStatusButton(element);
                this._reloadButton.addEventListener(WebInspector.TreeElementStatusButton.Event.Clicked, this._reloadPageClicked, this);
                loadedImages.call(this);
            }.bind(this));

            wrappedSVGDocument(platformImagePath("DownloadArrow.svg"), null, WebInspector.UIString("Download Web Archive"), function(element) {
                this._downloadButton = new WebInspector.TreeElementStatusButton(element);
                this._downloadButton.addEventListener(WebInspector.TreeElementStatusButton.Event.Clicked, this._downloadButtonClicked, this);
                this._updateDownloadButton();
                loadedImages.call(this);
            }.bind(this));
        }
    }

    // Overrides from TreeElement (Private).

    onpopulate()
    {
        if (this.children.length && !this.shouldRefreshChildren)
            return;
        this.shouldRefreshChildren = false;

        this.removeChildren();
        this.updateParentStatus();
        this.prepareToPopulate();

        for (var i = 0; i < this._frame.childFrames.length; ++i)
            this.addChildForRepresentedObject(this._frame.childFrames[i]);

        for (var i = 0; i < this._frame.resources.length; ++i)
            this.addChildForRepresentedObject(this._frame.resources[i]);

        var sourceMaps = this.resource && this.resource.sourceMaps;
        for (var i = 0; i < sourceMaps.length; ++i) {
            var sourceMap = sourceMaps[i];
            for (var j = 0; j < sourceMap.resources.length; ++j)
                this.addChildForRepresentedObject(sourceMap.resources[j]);
        }

        var flowMap = this._frame.domTree.flowMap;
        for (var flowKey in flowMap)
            this.addChildForRepresentedObject(flowMap[flowKey]);

        var canvases = WebInspector.canvasManager.canvasesForFrame(this._frame);
        for (var canvas of canvases)
            this.addChildForRepresentedObject(canvas);
    }
        
    onexpand()
    {
        this._expandedSetting.value = true;
        this._frame.domTree.requestContentFlowList();
    }

    oncollapse()
    {
        // Only store the setting if we have children, since setting hasChildren to false will cause a collapse,
        // and we only care about user triggered collapses.
        if (this.hasChildren)
            this._expandedSetting.value = false;
    }

    // Private

    _updateExpandedSetting()
    {
        this._expandedSetting = new WebInspector.Setting("frame-expanded-" + this._frame.url.hash, this._frame.isMainFrame() ? true : false);
        if (this._expandedSetting.value)
            this.expand();
        else
            this.collapse();
    }

    _mainResourceDidChange(event)
    {
        this._updateResource(this._frame.mainResource);

        this.updateParentStatus();
        this.removeChildren();

        // Change the expanded setting since the frame URL has changed. Do this before setting shouldRefreshChildren, since
        // shouldRefreshChildren will call onpopulate if expanded is true.
        this._updateExpandedSetting();

        if (this._frame.isMainFrame())
            this._updateDownloadButton();

        this.shouldRefreshChildren = true;
    }

    _resourceWasAdded(event)
    {
        this.addRepresentedObjectToNewChildQueue(event.data.resource);
    }

    _resourceWasRemoved(event)
    {
        this.removeChildForRepresentedObject(event.data.resource);
    }

    _childFrameWasAdded(event)
    {
        this.addRepresentedObjectToNewChildQueue(event.data.childFrame);
    }

    _childFrameWasRemoved(event)
    {
        this.removeChildForRepresentedObject(event.data.childFrame);
    }

    _childContentFlowWasAdded(event)
    {
        this.addRepresentedObjectToNewChildQueue(event.data.flow);
    }

    _childContentFlowWasRemoved(event)
    {
        this.removeChildForRepresentedObject(event.data.flow);
    }

    _canvasesAvailable(event)
    {
        this.updateParentStatus();
        this.removeChildren();

        this.shouldRefreshChildren = true;
    }

    _canvasWasAdded(event)
    {
        var canvas = event.data.canvas;
        if (canvas.parentFrame == this._frame)
            this.addRepresentedObjectToNewChildQueue(canvas);
    }

    _canvasWasRemoved(event)
    {
        var canvas = event.data.canvas;
        if (canvas.parentFrame == this._frame)
            this.removeChildForRepresentedObject(canvas);
    }

    _rootDOMNodeInvalidated()
    {
        if (this.expanded)
            this._frame.domTree.requestContentFlowList();
    }

    _reloadPageClicked(event)
    {
        // Ignore cache when the shift key is pressed.
        PageAgent.reload(event.data.shiftKey);
    }

    _downloadButtonClicked(event)
    {
        WebInspector.archiveMainFrame();
    }

    _updateDownloadButton()
    {
        console.assert(this._frame.isMainFrame());
        if (!this._downloadButton)
            return;

        if (!PageAgent.archive || WebInspector.debuggableType !== WebInspector.DebuggableType.Web) {
            this._downloadButton.hidden = true;
            return;
        }

        if (this._downloadingPage) {
            this._downloadButton.enabled = false;
            return;
        }

        this._downloadButton.enabled = WebInspector.canArchiveMainFrame();
    }

    _pageArchiveStarted(event)
    {
        this._downloadingPage = true;
        this._updateDownloadButton();
    }

    _pageArchiveEnded(event)
    {
        this._downloadingPage = false;
        this._updateDownloadButton();
    }
};
