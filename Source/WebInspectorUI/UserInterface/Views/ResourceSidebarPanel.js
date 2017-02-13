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

WebInspector.ResourceSidebarPanel = class ResourceSidebarPanel extends WebInspector.NavigationSidebarPanel
{
    constructor(contentBrowser)
    {
        super("resource", WebInspector.UIString("Resources"), true);

        this.contentBrowser = contentBrowser;

        this.filterBar.placeholder = WebInspector.UIString("Filter Resource List");

        this._navigationBar = new WebInspector.NavigationBar;
        this.addSubview(this._navigationBar);

        this._targetTreeElementMap = new Map;

        var scopeItemPrefix = "resource-sidebar-";
        var scopeBarItems = [];

        scopeBarItems.push(new WebInspector.ScopeBarItem(scopeItemPrefix + "type-all", WebInspector.UIString("All Resources"), true));

        for (var key in WebInspector.Resource.Type) {
            var value = WebInspector.Resource.Type[key];
            var scopeBarItem = new WebInspector.ScopeBarItem(scopeItemPrefix + value, WebInspector.Resource.displayNameForType(value, true));
            scopeBarItem[WebInspector.ResourceSidebarPanel.ResourceTypeSymbol] = value;
            scopeBarItems.push(scopeBarItem);
        }

        this._scopeBar = new WebInspector.ScopeBar("resource-sidebar-scope-bar", scopeBarItems, scopeBarItems[0], true);
        this._scopeBar.addEventListener(WebInspector.ScopeBar.Event.SelectionChanged, this._scopeBarSelectionDidChange, this);

        this._navigationBar.addNavigationItem(this._scopeBar);

        WebInspector.Frame.addEventListener(WebInspector.Frame.Event.MainResourceDidChange, this._mainResourceDidChange, this);

        WebInspector.frameResourceManager.addEventListener(WebInspector.FrameResourceManager.Event.MainFrameDidChange, this._mainFrameDidChange, this);

        WebInspector.debuggerManager.addEventListener(WebInspector.DebuggerManager.Event.ScriptAdded, this._scriptWasAdded, this);
        WebInspector.debuggerManager.addEventListener(WebInspector.DebuggerManager.Event.ScriptRemoved, this._scriptWasRemoved, this);
        WebInspector.debuggerManager.addEventListener(WebInspector.DebuggerManager.Event.ScriptsCleared, this._scriptsCleared, this);

        WebInspector.targetManager.addEventListener(WebInspector.TargetManager.Event.TargetRemoved, this._targetRemoved, this);

        WebInspector.notifications.addEventListener(WebInspector.Notification.ExtraDomainsActivated, this._extraDomainsActivated, this);

        this.contentTreeOutline.addEventListener(WebInspector.TreeOutline.Event.SelectionDidChange, this._treeSelectionDidChange, this);
        this.contentTreeOutline.includeSourceMapResourceChildren = true;

        if (WebInspector.debuggableType === WebInspector.DebuggableType.JavaScript) {
            this.contentTreeOutline.disclosureButtons = false;
            WebInspector.SourceCode.addEventListener(WebInspector.SourceCode.Event.SourceMapAdded, () => { this.contentTreeOutline.disclosureButtons = true; }, this);
        }

        if (WebInspector.frameResourceManager.mainFrame)
            this._mainFrameMainResourceDidChange(WebInspector.frameResourceManager.mainFrame);
    }

    // Public

    get minimumWidth()
    {
        return this._navigationBar.minimumWidth;
    }

    closed()
    {
        super.closed();

        WebInspector.Frame.removeEventListener(null, null, this);
        WebInspector.frameResourceManager.removeEventListener(null, null, this);
        WebInspector.debuggerManager.removeEventListener(null, null, this);
        WebInspector.notifications.removeEventListener(null, null, this);
    }

    showDefaultContentView()
    {
        if (WebInspector.frameResourceManager.mainFrame) {
            this.contentBrowser.showContentViewForRepresentedObject(WebInspector.frameResourceManager.mainFrame);
            return;
        }

        var firstTreeElement = this.contentTreeOutline.children[0];
        if (firstTreeElement)
            this.showDefaultContentViewForTreeElement(firstTreeElement);
    }

    treeElementForRepresentedObject(representedObject)
    {
        // A custom implementation is needed for this since the frames are populated lazily.

        if (!this._mainFrameTreeElement && (representedObject instanceof WebInspector.Resource || representedObject instanceof WebInspector.Frame || representedObject instanceof WebInspector.Collection)) {
            // All resources are under the main frame, so we need to return early if we don't have the main frame yet.
            return null;
        }

        // The Frame is used as the representedObject instead of the main resource in our tree.
        if (representedObject instanceof WebInspector.Resource && representedObject.parentFrame && representedObject.parentFrame.mainResource === representedObject)
            representedObject = representedObject.parentFrame;

        function isAncestor(ancestor, resourceOrFrame)
        {
            // SourceMapResources are descendants of another SourceCode object.
            if (resourceOrFrame instanceof WebInspector.SourceMapResource) {
                if (resourceOrFrame.sourceMap.originalSourceCode === ancestor)
                    return true;

                // Not a direct ancestor, so check the ancestors of the parent SourceCode object.
                resourceOrFrame = resourceOrFrame.sourceMap.originalSourceCode;
            }

            var currentFrame = resourceOrFrame.parentFrame;
            while (currentFrame) {
                if (currentFrame === ancestor)
                    return true;
                currentFrame = currentFrame.parentFrame;
            }

            return false;
        }

        function getParent(resourceOrFrame)
        {
            // SourceMapResources are descendants of another SourceCode object.
            if (resourceOrFrame instanceof WebInspector.SourceMapResource)
                return resourceOrFrame.sourceMap.originalSourceCode;
            return resourceOrFrame.parentFrame;
        }

        var treeElement = this.contentTreeOutline.findTreeElement(representedObject, isAncestor, getParent);
        if (treeElement)
            return treeElement;

        // Only special case Script objects.
        if (!(representedObject instanceof WebInspector.Script)) {
            console.error("Didn't find a TreeElement for representedObject", representedObject);
            return null;
        }

        // If the Script has a URL we should have found it earlier.
        if (representedObject.url) {
            console.error("Didn't find a ScriptTreeElement for a Script with a URL.");
            return null;
        }

        // Since the Script does not have a URL we consider it an 'anonymous' script. These scripts happen from calls to
        // window.eval() or browser features like Auto Fill and Reader. They are not normally added to the sidebar, but since
        // we have a ScriptContentView asking for the tree element we will make a ScriptTreeElement on demand and add it.

        if (!this._anonymousScriptsFolderTreeElement) {
            let collection = new WebInspector.Collection(WebInspector.Collection.TypeVerifier.Script);
            this._anonymousScriptsFolderTreeElement = new WebInspector.FolderTreeElement(WebInspector.UIString("Anonymous Scripts"), collection);
        }

        if (!this._anonymousScriptsFolderTreeElement.parent) {
            var index = insertionIndexForObjectInListSortedByFunction(this._anonymousScriptsFolderTreeElement, this.contentTreeOutline.children, this._compareTreeElements);
            this.contentTreeOutline.insertChild(this._anonymousScriptsFolderTreeElement, index);
        }

        this._anonymousScriptsFolderTreeElement.representedObject.add(representedObject);

        var scriptTreeElement = new WebInspector.ScriptTreeElement(representedObject);
        this._anonymousScriptsFolderTreeElement.appendChild(scriptTreeElement);

        return scriptTreeElement;
    }

    // Protected

    hasCustomFilters()
    {
        console.assert(this._scopeBar.selectedItems.length === 1);
        var selectedScopeBarItem = this._scopeBar.selectedItems[0];
        return selectedScopeBarItem && !selectedScopeBarItem.exclusive;
    }

    matchTreeElementAgainstCustomFilters(treeElement, flags)
    {
        console.assert(this._scopeBar.selectedItems.length === 1);
        var selectedScopeBarItem = this._scopeBar.selectedItems[0];

        // Show everything if there is no selection or "All Resources" is selected (the exclusive item).
        if (!selectedScopeBarItem || selectedScopeBarItem.exclusive)
            return true;

        // Folders are hidden on the first pass, but visible childen under the folder will force the folder visible again.
        if (treeElement instanceof WebInspector.FolderTreeElement)
            return false;

        function match()
        {
            if (treeElement instanceof WebInspector.FrameTreeElement)
                return selectedScopeBarItem[WebInspector.ResourceSidebarPanel.ResourceTypeSymbol] === WebInspector.Resource.Type.Document;

            if (treeElement instanceof WebInspector.ScriptTreeElement)
                return selectedScopeBarItem[WebInspector.ResourceSidebarPanel.ResourceTypeSymbol] === WebInspector.Resource.Type.Script;

            console.assert(treeElement instanceof WebInspector.ResourceTreeElement, "Unknown treeElement", treeElement);
            if (!(treeElement instanceof WebInspector.ResourceTreeElement))
                return false;

            return treeElement.resource.type === selectedScopeBarItem[WebInspector.ResourceSidebarPanel.ResourceTypeSymbol];
        }

        var matched = match();
        if (matched)
            flags.expandTreeElement = true;
        return matched;
    }

    // Private

    _mainResourceDidChange(event)
    {
        if (!event.target.isMainFrame())
            return;

        this._mainFrameMainResourceDidChange(event.target);
    }

    _mainFrameDidChange(event)
    {
        this._mainFrameMainResourceDidChange(WebInspector.frameResourceManager.mainFrame);
    }

    _mainFrameMainResourceDidChange(mainFrame)
    {
        this.contentBrowser.contentViewContainer.closeAllContentViews();

        if (this._mainFrameTreeElement) {
            this.contentTreeOutline.removeChild(this._mainFrameTreeElement);
            this._mainFrameTreeElement = null;
        }

        if (!mainFrame)
            return;

        this._mainFrameTreeElement = new WebInspector.FrameTreeElement(mainFrame);
        this.contentTreeOutline.insertChild(this._mainFrameTreeElement, 0);

        function delayedWork()
        {
            if (!this.contentTreeOutline.selectedTreeElement) {
                var currentContentView = this.contentBrowser.currentContentView;
                var treeElement = currentContentView ? this.treeElementForRepresentedObject(currentContentView.representedObject) : null;
                if (!treeElement)
                    treeElement = this._mainFrameTreeElement;
                this.showDefaultContentViewForTreeElement(treeElement);
            }
        }

        // Cookie restoration will attempt to re-select the resource we were showing.
        // Give it time to do that before selecting the main frame resource.
        setTimeout(delayedWork.bind(this));
    }

    _scriptWasAdded(event)
    {
        var script = event.data.script;

        // We don't add scripts without URLs here. Those scripts can quickly clutter the interface and
        // are usually more transient. They will get added if/when they need to be shown in a content view.
        if (!script.url && !script.sourceURL)
            return;

        // Worker script.
        if (script.target !== WebInspector.mainTarget) {
            if (script.isMainResource())
                this._addTargetWithMainResource(script.target);
            return;
        }

        // If the script URL matches a resource we can assume it is part of that resource and does not need added.
        if (script.resource || script.dynamicallyAddedScriptElement)
            return;

        let insertIntoTopLevel = false;
        let parentFolderTreeElement = null;

        if (script.injected) {
            if (!this._extensionScriptsFolderTreeElement) {
                let collection = new WebInspector.Collection(WebInspector.Collection.TypeVerifier.Script);
                this._extensionScriptsFolderTreeElement = new WebInspector.FolderTreeElement(WebInspector.UIString("Extension Scripts"), collection);
            }

            parentFolderTreeElement = this._extensionScriptsFolderTreeElement;
        } else {
            if (WebInspector.debuggableType === WebInspector.DebuggableType.JavaScript && !WebInspector.hasExtraDomains)
                insertIntoTopLevel = true;
            else {
                if (!this._extraScriptsFolderTreeElement) {
                    let collection = new WebInspector.Collection(WebInspector.Collection.TypeVerifier.Script);
                    this._extraScriptsFolderTreeElement = new WebInspector.FolderTreeElement(WebInspector.UIString("Extra Scripts"), collection);
                }

                parentFolderTreeElement = this._extraScriptsFolderTreeElement;
            }
        }

        if (parentFolderTreeElement)
            parentFolderTreeElement.representedObject.add(script);

        var scriptTreeElement = new WebInspector.ScriptTreeElement(script);

        if (insertIntoTopLevel) {
            var index = insertionIndexForObjectInListSortedByFunction(scriptTreeElement, this.contentTreeOutline.children, this._compareTreeElements);
            this.contentTreeOutline.insertChild(scriptTreeElement, index);
        } else {
            if (!parentFolderTreeElement.parent) {
                var index = insertionIndexForObjectInListSortedByFunction(parentFolderTreeElement, this.contentTreeOutline.children, this._compareTreeElements);
                this.contentTreeOutline.insertChild(parentFolderTreeElement, index);
            }

            parentFolderTreeElement.appendChild(scriptTreeElement);
        }
    }

    _scriptWasRemoved(event)
    {
        let script = event.data.script;
        let scriptTreeElement = this.contentTreeOutline.getCachedTreeElement(script);
        if (!scriptTreeElement)
            return;

        let parentTreeElement = scriptTreeElement.parent;
        parentTreeElement.removeChild(scriptTreeElement);

        if (parentTreeElement instanceof WebInspector.FolderTreeElement) {
            parentTreeElement.representedObject.remove(script);

            if (!parentTreeElement.children.length)
                parentTreeElement.parent.removeChild(parentTreeElement);
        }
    }

    _scriptsCleared(event)
    {
        const suppressOnDeselect = true;
        const suppressSelectSibling = true;

        if (this._extensionScriptsFolderTreeElement) {
            if (this._extensionScriptsFolderTreeElement.parent)
                this._extensionScriptsFolderTreeElement.parent.removeChild(this._extensionScriptsFolderTreeElement, suppressOnDeselect, suppressSelectSibling);

            this._extensionScriptsFolderTreeElement.representedObject.clear();
            this._extensionScriptsFolderTreeElement = null;
        }

        if (this._extraScriptsFolderTreeElement) {
            if (this._extraScriptsFolderTreeElement.parent)
                this._extraScriptsFolderTreeElement.parent.removeChild(this._extraScriptsFolderTreeElement, suppressOnDeselect, suppressSelectSibling);

            this._extraScriptsFolderTreeElement.representedObject.clear();
            this._extraScriptsFolderTreeElement = null;
        }

        if (this._anonymousScriptsFolderTreeElement) {
            if (this._anonymousScriptsFolderTreeElement.parent)
                this._anonymousScriptsFolderTreeElement.parent.removeChild(this._anonymousScriptsFolderTreeElement, suppressOnDeselect, suppressSelectSibling);

            this._anonymousScriptsFolderTreeElement.representedObject.clear();
            this._anonymousScriptsFolderTreeElement = null;
        }

        if (this._targetTreeElementMap.size) {
            for (let treeElement of this._targetTreeElementMap)
                treeElement.parent.removeChild(treeElement, suppressOnDeselect, suppressSelectSibling);
            this._targetTreeElementMap.clear();
        }
    }

    _addTargetWithMainResource(target)
    {
        console.assert(target.type === WebInspector.Target.Type.Worker);

        let targetTreeElement = new WebInspector.WorkerTreeElement(target);
        this._targetTreeElementMap.set(target, targetTreeElement);

        let index = insertionIndexForObjectInListSortedByFunction(targetTreeElement, this.contentTreeOutline.children, this._compareTreeElements);
        this.contentTreeOutline.insertChild(targetTreeElement, index);
    }

    _targetRemoved(event)
    {
        let removedTarget = event.data.target;

        let targetTreeElement = this._targetTreeElementMap.take(removedTarget);
        if (targetTreeElement)
            targetTreeElement.parent.removeChild(targetTreeElement);
    }

    _treeSelectionDidChange(event)
    {
        let treeElement = event.data.selectedElement;
        if (!treeElement)
            return;

        if (treeElement instanceof WebInspector.FolderTreeElement
            || treeElement instanceof WebInspector.ResourceTreeElement
            || treeElement instanceof WebInspector.ScriptTreeElement
            || treeElement instanceof WebInspector.ContentFlowTreeElement) {
            WebInspector.showRepresentedObject(treeElement.representedObject);
            return;
        }

        console.error("Unknown tree element", treeElement);
    }

    _compareTreeElements(a, b)
    {
        // Always sort the main frame element first.
        if (a instanceof WebInspector.FrameTreeElement)
            return -1;
        if (b instanceof WebInspector.FrameTreeElement)
            return 1;

        console.assert(a.mainTitle);
        console.assert(b.mainTitle);

        return (a.mainTitle || "").localeCompare(b.mainTitle || "");
    }

    _extraDomainsActivated()
    {
        if (WebInspector.debuggableType === WebInspector.DebuggableType.JavaScript)
            this.contentTreeOutline.disclosureButtons = true;
    }

    _scopeBarSelectionDidChange(event)
    {
        this.updateFilter();
    }
};

WebInspector.ResourceSidebarPanel.ResourceTypeSymbol = Symbol("resource-type");
