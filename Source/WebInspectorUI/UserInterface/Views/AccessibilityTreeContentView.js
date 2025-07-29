/*
 * Copyright (C) 2025 Apple Inc. All rights reserved.
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

WI.AccessibilityTreeContentView = class AccessibilityTreeContentView extends WI.ContentView
{
    constructor(representedObject)
    {
        super(representedObject);

        this.element.classList.add("dom-tree");
        this.element.addEventListener("click", this._mouseWasClicked.bind(this), false);

        this._domTreeOutline = new WI.DOMTreeOutline({omitRootDOMNode: true, excludeRevealElementContextMenu: true, showInspectedNode: true, showBadges: true});
        this.domTreeOutline.isAccessibilityTree = true;
        this._domTreeOutline.allowsEmptySelection = false;
        this._domTreeOutline.allowsMultipleSelection = true;
        this._domTreeOutline.addEventListener(WI.TreeOutline.Event.ElementAdded, this._domTreeElementAdded, this);
        this._domTreeOutline.addEventListener(WI.TreeOutline.Event.SelectionDidChange, this._domTreeSelectionDidChange, this);
        this._domTreeOutline.addEventListener(WI.DOMTreeOutline.Event.SelectedNodeChanged, this._selectedNodeDidChange, this);
        this._domTreeOutline.wireToDomAgent();
        this._domTreeOutline.editable = true;
        this.element.appendChild(this._domTreeOutline.element);

        WI.domManager.addEventListener(WI.DOMManager.Event.AttributeModified, this._domNodeChanged, this);
        WI.domManager.addEventListener(WI.DOMManager.Event.AttributeRemoved, this._domNodeChanged, this);
        WI.domManager.addEventListener(WI.DOMManager.Event.CharacterDataModified, this._domNodeChanged, this);

        WI.settings.domTreeDeemphasizesNodesThatAreNotRendered.addEventListener(WI.Setting.Event.Changed, this._handleDOMTreeDeemphasizesNodesThatAreNotRenderedChanged, this);
        this._updateDOMTreeDeemphasizesNodesThatAreNotRendered();
    }

    // Public
    get domTreeOutline()
    {
        return this._domTreeOutline;
    }

    get scrollableElements()
    {
        return [this.element];
    }

    closed()
    {
        super.closed();
        this._domTreeOutline.close();
    }

    get selectionPathComponents()
    {
        var treeElement = this._domTreeOutline.selectedTreeElement;
        var pathComponents = [];

        while (treeElement && !treeElement.root) {
            // The close tag is contained within the element it closes. So skip it since we don't want to
            // show the same node twice in the hierarchy.
            if (treeElement.isCloseTag()) {
                treeElement = treeElement.parent;
                continue;
            }

            var pathComponent = new WI.DOMTreeElementPathComponent(treeElement, treeElement.representedObject);
            pathComponent.addEventListener(WI.HierarchicalPathComponent.Event.Clicked, this._handlePathComponentSelected, this);
            pathComponent.addEventListener(WI.HierarchicalPathComponent.Event.SiblingWasSelected, this._handlePathComponentSelected, this);
            pathComponents.unshift(pathComponent);
            treeElement = treeElement.parent;
        }
        return pathComponents;
    }

    restoreFromCookie(cookie)
    {
        if (!cookie || !cookie.nodeToSelect)
            return;

        this.selectAndRevealDOMNode(cookie.nodeToSelect);

        // Because nodeToSelect is ephemeral, we don't want to keep
        // it around in the back-forward history entries.
        cookie.nodeToSelect = undefined;
    }

    selectAndRevealDOMNode(domNode, preventFocusChange)
    {
        this._domTreeOutline.selectDOMNode(domNode, !preventFocusChange);
    }
    async handleCopyEvent(event)
    {
        let promises = [];

        for (let treeElement of this._domTreeOutline.selectedTreeElements) {
            if (treeElement.representedObject instanceof WI.DOMNode)
                promises.push(treeElement.representedObject.getOuterHTML());
        }

        if (!promises.length)
            return;

        event.clipboardData.clearData();
        event.preventDefault();

        let outerHTMLs = await Promise.all(promises);
        InspectorFrontendHost.copyText(outerHTMLs.join("\n"));
    }

    handlePasteEvent(event)
    {
        let selectedDOMNode = this._domTreeOutline.selectedDOMNode();
        if (!selectedDOMNode)
            return;

        let text = event.clipboardData.getData("text/plain");
        if (!text)
            return;

        selectedDOMNode.insertAdjacentHTML("afterend", text);
    }

    get supportsSave()
    {
        return WI.canArchiveMainFrame();
    }

    get saveMode()
    {
        return WI.FileUtilities.SaveMode.SingleFile;
    }

    get saveData()
    {
        return {customSaveHandler: () => { WI.archiveMainFrame(); }};
    }
    // Protected

    attached()
    {
        super.attached();

        this._domTreeOutline.setVisible(true, WI.isConsoleFocused());
    }

    detached()
    {
        WI.domManager.hideDOMNodeHighlight();
        this._domTreeOutline.setVisible(false);
        super.detached();
    }

    sizeDidChange()
    {
        super.sizeDidChange();
        this._domTreeOutline.selectDOMNode(this._domTreeOutline.selectedDOMNode());
    }

    layout()
    {
        super.layout();
        this._domTreeOutline.updateSelectionArea();
    }

    // Private

    _restoreSelectedNodeAfterUpdate(documentURL, defaultNode)
    {
        if (!WI.domManager.restoreSelectedNodeIsAllowed)
            return;

        function selectNode(lastSelectedNode)
        {
            var nodeToFocus = lastSelectedNode;
            if (!nodeToFocus)
                nodeToFocus = defaultNode;

            if (!nodeToFocus)
                return;

            this._dontSetLastSelectedNodePath = true;
            this.selectAndRevealDOMNode(nodeToFocus, WI.isConsoleFocused());
            this._dontSetLastSelectedNodePath = false;

            // If this wasn't the last selected node, then expand it.
            if (!lastSelectedNode && this._domTreeOutline.selectedTreeElement)
                this._domTreeOutline.selectedTreeElement.expand();
        }

        function selectLastSelectedNode(nodeId)
        {
            if (!WI.domManager.restoreSelectedNodeIsAllowed)
                return;

            selectNode.call(this, WI.domManager.nodeForId(nodeId));
        }

        selectNode.call(this);
    }



    _domTreeElementAdded(event)
    {
        let treeElement = event.data.element;
        let node = treeElement.representedObject;
        console.assert(node instanceof WI.DOMNode);
        if (!(node instanceof WI.DOMNode))
            return;
    }

    _domTreeSelectionDidChange(event)
    {
        let treeElement = this._domTreeOutline.selectedTreeElement;
        let domNode = treeElement ? treeElement.representedObject : null;
        let selectedByUser = event.data.selectedByUser;

        this._domTreeOutline.suppressRevealAndSelect = true;
        this._domTreeOutline.selectDOMNode(domNode, selectedByUser);

        if (domNode && selectedByUser)
            domNode.highlight();

        this._domTreeOutline.updateSelectionArea();
        this._domTreeOutline.suppressRevealAndSelect = false;
    }

    _selectedNodeDidChange(event)
    {
        var selectedDOMNode = this._domTreeOutline.selectedDOMNode();
    
        if (selectedDOMNode) {
            WI.domManager.highlightDOMNodeList([selectedDOMNode]);
        }

        this.dispatchEventToListeners(WI.ContentView.Event.SelectionPathComponentsDidChange);
    }

    _handlePathComponentSelected(event)
    {
        if (!event.data.pathComponent)
            return;

        console.assert(event.data.pathComponent instanceof WI.DOMTreeElementPathComponent);
        console.assert(event.data.pathComponent.domTreeElement instanceof WI.DOMTreeElement);

    }

    _domNodeChanged(event)
    {
        var selectedDOMNode = this._domTreeOutline.selectedDOMNode();
        if (selectedDOMNode !== event.data.node)
            return;

        this.dispatchEventToListeners(WI.ContentView.Event.SelectionPathComponentsDidChange);
    }

    _mouseWasClicked(event)
    {
        var anchorElement = event.target.closest("a");
        if (!anchorElement || !anchorElement.href)
            return;

        // Prevent the link from navigating, since we don't do any navigation by following links normally.
        event.preventDefault();
        event.stopPropagation();

        if (WI.isBeingEdited(anchorElement)) {
            // Don't follow the link when it is being edited.
            return;
        }

        // Cancel any pending link navigation.
        if (this._followLinkTimeoutIdentifier) {
            clearTimeout(this._followLinkTimeoutIdentifier);
            delete this._followLinkTimeoutIdentifier;
        }

        // If this is a double-click (or multiple-click), return early.
        if (event.detail > 1)
            return;

        function followLink()
        {
            // Since followLink is delayed, the call to WI.openURL can't look at window.event
            // to see if the command key is down like it normally would. So we need to do that check
            // before calling WI.openURL.
            const options = {
                alwaysOpenExternally: event?.metaKey ?? false,
                frame: this._frame,
                lineNumber: anchorElement.lineNumber,
                ignoreNetworkTab: true,
                ignoreSearchTab: true,
            };
            WI.openURL(anchorElement.href, options);
        }

        // Start a timeout since this is a single click, if the timeout is canceled before it fires,
        // then a double-click happened or another link was clicked.
        // FIXME: The duration might be longer or shorter than the user's configured double click speed.
        this._followLinkTimeoutIdentifier = setTimeout(followLink.bind(this), 333);
    }

    _updateDOMTreeDeemphasizesNodesThatAreNotRendered()
    {
        this.element.classList.toggle("deemphasize-unrendered", WI.settings.domTreeDeemphasizesNodesThatAreNotRendered.value);
    }

    _handleDOMTreeDeemphasizesNodesThatAreNotRenderedChanged(event)
    {
        this._updateDOMTreeDeemphasizesNodesThatAreNotRendered()
    }
};
