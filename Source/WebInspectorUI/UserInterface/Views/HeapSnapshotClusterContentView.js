/*
 * Copyright (C) 2016 Apple Inc. All rights reserved.
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

WebInspector.HeapSnapshotClusterContentView = class HeapSnapshotClusterContentView extends WebInspector.ClusterContentView
{
    constructor(heapSnapshot)
    {
        super(heapSnapshot);

        console.assert(heapSnapshot instanceof WebInspector.HeapSnapshot);

        this._heapSnapshot = heapSnapshot;

        function createPathComponent(displayName, className, identifier)
        {
            let pathComponent = new WebInspector.HierarchicalPathComponent(displayName, className, identifier, false, true);
            pathComponent.addEventListener(WebInspector.HierarchicalPathComponent.Event.SiblingWasSelected, this._pathComponentSelected, this);
            return pathComponent;
        }

        this._shownInitialContent = false;
        this._summaryContentView = null;
        this._instancesContentView = null;

        if (!WebInspector.HeapSnapshotClusterContentView.showInternalObjectsSetting)
            WebInspector.HeapSnapshotClusterContentView.showInternalObjectsSetting = new WebInspector.Setting("heap-snapshot-cluster-content-view-show-internal-objects", false);

        // FIXME: Need an image for showing / hiding internal objects.
        this._showInternalObjectsButtonNavigationItem = new WebInspector.ActivateButtonNavigationItem("show-internal-objects", WebInspector.UIString("Show internal objects"), WebInspector.UIString("Hide internal objects"), "Images/ShadowDOM.svg", 13, 13);
        this._showInternalObjectsButtonNavigationItem.addEventListener(WebInspector.ButtonNavigationItem.Event.Clicked, this._toggleShowInternalObjectsSetting, this);
        this._updateShowInternalObjectsButtonNavigationItem();

        this._summaryPathComponent = createPathComponent.call(this, WebInspector.UIString("Summary"), "heap-snapshot-summary-icon", WebInspector.HeapSnapshotClusterContentView.SummaryIdentifier);
        this._instancesPathComponent = createPathComponent.call(this, WebInspector.UIString("Instances"), "heap-snapshot-instances-icon", WebInspector.HeapSnapshotClusterContentView.InstancesIdentifier);

        this._summaryPathComponent.nextSibling = this._instancesPathComponent;
        this._instancesPathComponent.previousSibling = this._summaryPathComponent;

        this._currentContentViewSetting = new WebInspector.Setting("heap-snapshot-cluster-current-view", WebInspector.HeapSnapshotClusterContentView.SummaryIdentifier);
    }

    // Static

    static iconStyleClassNameForClassName(className, internal)
    {
        if (internal)
            return "unknown";

        switch (className) {
        case "Object":
        case "Array":
        case "Map":
        case "Set":
        case "WeakMap":
        case "WeakSet":
        case "Promise":
        case "Error":
        case "Window":
        case "Map Iterator":
        case "Set Iterator":
        case "Math":
        case "JSON":
        case "GlobalObject":
            return "object";
        case "Function":
            return "function";
        case "RegExp":
            return "regex";
        case "Number":
            return "number";
        case "Boolean":
            return "boolean";
        case "String":
        case "string":
            return "string";
        case "Symbol":
        case "symbol":
            return "symbol";
        }

        if (className.endsWith("Prototype"))
            return "object";
        if (className.endsWith("Element") || className === "Node" || className === "Text")
            return "node";

        return "native";
    }

    // Public

    get heapSnapshot() { return this._heapSnapshot; }

    get summaryContentView()
    {
        if (!this._summaryContentView)
            this._summaryContentView = new WebInspector.HeapSnapshotSummaryContentView(this._heapSnapshot, this._contentViewExtraArguments());
        return this._summaryContentView;
    }

    get instancesContentView()
    {
        if (!this._instancesContentView)
            this._instancesContentView = new WebInspector.HeapSnapshotInstancesContentView(this._heapSnapshot, this._contentViewExtraArguments());
        return this._instancesContentView;
    }

    get navigationItems()
    {
        return [this._showInternalObjectsButtonNavigationItem];
    }

    get selectionPathComponents()
    {
        let currentContentView = this._contentViewContainer.currentContentView;
        if (!currentContentView)
            return [];

        let components = [this._pathComponentForContentView(currentContentView)];
        return components.concat(currentContentView.selectionPathComponents);
    }

    shown()
    {
        super.shown();

        if (this._shownInitialContent) {
            this._updateViewsForShowInternalObjectsSettingValue();
            return;
        }

        this._showContentViewForIdentifier(this._currentContentViewSetting.value);
    }

    closed()
    {
        super.closed();

        this._shownInitialContent = false;
    }

    saveToCookie(cookie)
    {
        cookie[WebInspector.HeapSnapshotClusterContentView.ContentViewIdentifierCookieKey] = this._currentContentViewSetting.value;
    }

    restoreFromCookie(cookie)
    {
        this._showContentViewForIdentifier(cookie[WebInspector.HeapSnapshotClusterContentView.ContentViewIdentifierCookieKey]);
    }

    showSummary()
    {
        this._shownInitialContent = true;
        return this._showContentViewForIdentifier(WebInspector.HeapSnapshotClusterContentView.SummaryIdentifier);
    }

    showInstances()
    {
        this._shownInitialContent = true;
        return this._showContentViewForIdentifier(WebInspector.HeapSnapshotClusterContentView.InstancesIdentifier);
    }

    // Private

    _contentViewExtraArguments()
    {
        return {showInternalObjects: WebInspector.HeapSnapshotClusterContentView.showInternalObjectsSetting.value};
    }

    _pathComponentForContentView(contentView)
    {
        console.assert(contentView);
        if (!contentView)
            return null;
        if (contentView === this._summaryContentView)
            return this._summaryPathComponent;
        if (contentView === this._instancesContentView)
            return this._instancesPathComponent;
        console.error("Unknown contentView.");
        return null;
    }

    _identifierForContentView(contentView)
    {
        console.assert(contentView);
        if (!contentView)
            return null;
        if (contentView === this._summaryContentView)
            return WebInspector.HeapSnapshotClusterContentView.SummaryIdentifier;
        if (contentView === this._instancesContentView)
            return WebInspector.HeapSnapshotClusterContentView.InstancesIdentifier;
        console.error("Unknown contentView.");
        return null;
    }

    _showContentViewForIdentifier(identifier)
    {
        let contentViewToShow = null;

        switch (identifier) {
        case WebInspector.HeapSnapshotClusterContentView.SummaryIdentifier:
            contentViewToShow = this.summaryContentView;
            break;
        case WebInspector.HeapSnapshotClusterContentView.InstancesIdentifier:
            contentViewToShow = this.instancesContentView;
            break;
        }

        if (!contentViewToShow)
            contentViewToShow = this.instancesContentView;

        console.assert(contentViewToShow);

        this._shownInitialContent = true;

        this._currentContentViewSetting.value = this._identifierForContentView(contentViewToShow);

        return this.contentViewContainer.showContentView(contentViewToShow);
    }

    _pathComponentSelected(event)
    {
        this._showContentViewForIdentifier(event.data.pathComponent.representedObject);
    }

    _toggleShowInternalObjectsSetting(event)
    {
        WebInspector.HeapSnapshotClusterContentView.showInternalObjectsSetting.value = !WebInspector.HeapSnapshotClusterContentView.showInternalObjectsSetting.value;

        this._updateViewsForShowInternalObjectsSettingValue();
    }

    _updateViewsForShowInternalObjectsSettingValue(force)
    {
        let value = WebInspector.HeapSnapshotClusterContentView.showInternalObjectsSetting.value;
        if (this._showInternalObjectsButtonNavigationItem.activated === value)
            return;

        if (this._instancesContentView)
            this._instancesContentView.showInternalObjects = value;

        this._updateShowInternalObjectsButtonNavigationItem();
    }

    _updateShowInternalObjectsButtonNavigationItem()
    {
        this._showInternalObjectsButtonNavigationItem.activated = WebInspector.HeapSnapshotClusterContentView.showInternalObjectsSetting.value;
    }
};

WebInspector.HeapSnapshotClusterContentView.ContentViewIdentifierCookieKey = "heap-snapshot-cluster-content-view-identifier";

WebInspector.HeapSnapshotClusterContentView.SummaryIdentifier = "summary";
WebInspector.HeapSnapshotClusterContentView.InstancesIdentifier = "instances";
