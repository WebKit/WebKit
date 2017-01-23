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
        frame.addEventListener(WebInspector.Frame.Event.ExtraScriptAdded, this._extraScriptAdded, this);
        frame.addEventListener(WebInspector.Frame.Event.ChildFrameWasAdded, this._childFrameWasAdded, this);
        frame.addEventListener(WebInspector.Frame.Event.ChildFrameWasRemoved, this._childFrameWasRemoved, this);

        frame.domTree.addEventListener(WebInspector.DOMTree.Event.ContentFlowWasAdded, this._childContentFlowWasAdded, this);
        frame.domTree.addEventListener(WebInspector.DOMTree.Event.ContentFlowWasRemoved, this._childContentFlowWasRemoved, this);
        frame.domTree.addEventListener(WebInspector.DOMTree.Event.RootDOMNodeInvalidated, this._rootDOMNodeInvalidated, this);

        this.shouldRefreshChildren = true;
        this.folderSettingsKey = this._frame.url.hash;

        this.registerFolderizeSettings("frames", WebInspector.UIString("Frames"), this._frame.childFrameCollection, WebInspector.FrameTreeElement);
        this.registerFolderizeSettings("flows", WebInspector.UIString("Flows"), this._frame.domTree.contentFlowCollection, WebInspector.ContentFlowTreeElement);
        this.registerFolderizeSettings("extra-scripts", WebInspector.UIString("Extra Scripts"), this._frame.extraScriptCollection, WebInspector.ScriptTreeElement);

        for (let [key, value] of Object.entries(WebInspector.Resource.Type)) {
            let folderName = WebInspector.Resource.displayNameForType(value, true);
            this.registerFolderizeSettings(key, folderName, this._frame.resourceCollectionForType(value), WebInspector.ResourceTreeElement);
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

    // Overrides from TreeElement (Private).

    onpopulate()
    {
        if (this.children.length && !this.shouldRefreshChildren)
            return;
        this.shouldRefreshChildren = false;

        this.removeChildren();
        this.updateParentStatus();
        this.prepareToPopulate();

        for (let frame of this._frame.childFrameCollection.items)
            this.addChildForRepresentedObject(frame);

        for (let resource of this._frame.resourceCollection.items)
            this.addChildForRepresentedObject(resource);

        var sourceMaps = this.resource && this.resource.sourceMaps;
        for (var i = 0; i < sourceMaps.length; ++i) {
            var sourceMap = sourceMaps[i];
            for (var j = 0; j < sourceMap.resources.length; ++j)
                this.addChildForRepresentedObject(sourceMap.resources[j]);
        }

        for (let contentFlow of this._frame.domTree.contentFlowCollection.items)
            this.addChildForRepresentedObject(contentFlow);

        for (let extraScript of this._frame.extraScriptCollection.items) {
            if (extraScript.sourceURL || extraScript.sourceMappingURL)
                this.addChildForRepresentedObject(extraScript);
        }
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

    _extraScriptAdded(event)
    {
        let extraScript = event.data.script;
        if (extraScript.sourceURL || extraScript.sourceMappingURL)
            this.addRepresentedObjectToNewChildQueue(extraScript);
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

    _rootDOMNodeInvalidated()
    {
        if (this.expanded)
            this._frame.domTree.requestContentFlowList();
    }
};
