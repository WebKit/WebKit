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

WI.AccessibilityDetailsSidebarPanel = class AccessibilityDetailsSidebarPanel extends WI.DOMDetailsSidebarPanel
{
    constructor()
    {
        super("accessibility-details", WI.UIString("Accessibility"));

        this.element.classList.add("dom-node");
    }

    // Public
    closed()
    {
        if (this.didInitialLayout)
            WI.domManager.removeEventListener(WI.DOMManager.Event.AttributeModified, this._attributesChanged, this);

        super.closed();
    }

    // Protected

    supportsDOMNode(nodeToInspect)
    {
        if (nodeToInspect.nodeType() !== Node.ELEMENT_NODE)
            return false;
            
        let tabContentView = WI.tabBrowser?.selectedTabContentView;
        if (tabContentView?.contentBrowser) {
            let currentContentView = tabContentView.contentBrowser.currentContentView;
            return currentContentView?.representedObject instanceof WI.AccessibilityTree;
        }
        return false;
    }

    initialLayout()
    {
        super.initialLayout();

        WI.domManager.addEventListener(WI.DOMManager.Event.AttributeModified, this._attributesChanged, this);

        this._identityNodeTypeRow = new WI.DetailsSectionSimpleRow(WI.UIString("Type"));
        this._identityNodeNameRow = new WI.DetailsSectionSimpleRow(WI.UIString("Name"));
        this._identityNodeValueRow = new WI.DetailsSectionSimpleRow(WI.UIString("Value"));
        this._identityNodeContentSecurityPolicyHashRow = new WI.DetailsSectionSimpleRow(WI.UIString("CSP Hash"));

        var identityGroup = new WI.DetailsSectionGroup([this._identityNodeTypeRow, this._identityNodeNameRow, this._identityNodeValueRow, this._identityNodeContentSecurityPolicyHashRow]);
        var identitySection = new WI.DetailsSection("dom-node-identity", WI.UIString("Identity"), [identityGroup]);
        this.contentView.element.appendChild(identitySection.element);

       if (this._accessibilitySupported()) {
            this._accessibilitySection = new WI.AccessibilityDetailsSection(this.domNode);
            this.contentView.element.appendChild(this._accessibilitySection.element);
       }

    }

    layout()
    {
        super.layout();

        if (!this.domNode || this.domNode.destroyed)
            return;

        this._refreshIdentity();
        if (this._accessibilitySection) {
            this._accessibilitySection.domNode = this.domNode;
            this._accessibilitySection._refreshProperties();
        }
    }

    // Private

    _accessibilitySupported()
    {
        return InspectorBackend.hasCommand("DOM.getAccessibilityPropertiesForNode");
    }

    _refreshIdentity()
    {
        const domNode = this.domNode;
        this._identityNodeTypeRow.value = this._nodeTypeDisplayName();
        this._identityNodeNameRow.value = domNode.nodeNameInCorrectCase();
        this._identityNodeValueRow.value = domNode.nodeValue();
        this._identityNodeContentSecurityPolicyHashRow.value = domNode.contentSecurityPolicyHash();
    }

    _attributesChanged(event)
    {
        if (event.data.node !== this.domNode)
            return;
        this._refreshAttributes();
        this._accessibilitySection._refreshProperties();
    }

    _nodeTypeDisplayName()
    {
        switch (this.domNode.nodeType()) {
        case Node.ELEMENT_NODE: {
            const nodeName = WI.UIString("Element");
            const state = this._customElementState();
            return state === null ? nodeName : `${nodeName} (${state})`;
        }
        case Node.TEXT_NODE:
            return WI.UIString("Text Node");
        case Node.COMMENT_NODE:
            return WI.UIString("Comment");
        case Node.DOCUMENT_NODE:
            return WI.UIString("Document");
        case Node.DOCUMENT_TYPE_NODE:
            return WI.UIString("Document Type");
        case Node.DOCUMENT_FRAGMENT_NODE:
            return WI.UIString("Document Fragment");
        case Node.CDATA_SECTION_NODE:
            return WI.UIString("Character Data");
        case Node.PROCESSING_INSTRUCTION_NODE:
            return WI.UIString("Processing Instruction");
        default:
            console.error("Unknown DOM node type: ", this.domNode.nodeType());
            return this.domNode.nodeType();
        }
    }

    _customElementState()
    {
        const state = this.domNode.customElementState();
        switch (state) {
        case WI.DOMNode.CustomElementState.Builtin:
            return null;
        case WI.DOMNode.CustomElementState.Custom:
            return WI.UIString("Custom");
        case WI.DOMNode.CustomElementState.Waiting:
            return WI.UIString("Undefined custom element");
        case WI.DOMNode.CustomElementState.Failed:
            return WI.UIString("Failed to upgrade");
        }
        console.error("Unknown DOM custom element state: ", state);
        return null;
    }
};

