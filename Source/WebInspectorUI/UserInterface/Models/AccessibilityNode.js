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

WI.AccessibilityNode = class AccessibilityNode extends WI.Object
{
    constructor(domNode, accessibilityProperties)
    {
        super();

        console.assert(domNode instanceof WI.DOMNode);
        console.assert(typeof accessibilityProperties === "object");

        this._domNode = domNode;
        this._properties = accessibilityProperties;
        this._computedRole = null;
        this._computedLabel = null;
        this._isIgnored = null;
        this._warningMessages = [];

        this._computeDerivedProperties();
    }

    // Public

    get domNode() { return this._domNode; }
    get properties() { return this._properties; }
    get computedRole() { return this._computedRole; }
    get computedLabel() { return this._computedLabel; }
    get isIgnored() { return this._isIgnored; }
    get hasWarnings() { return this._warningMessages.length > 0; }
    get warningMessages() { return this._warningMessages; }

    get role() { return this._properties.role; }
    get label() { return this._properties.label; }
    get focused() { return this._properties.focused; }
    get ignored() { return this._properties.ignored; }
    get ignoredByDefault() { return this._properties.ignoredByDefault; }
    get childNodeIds() { return this._properties.childNodeIds; }

    // Private

    _computeDerivedProperties()
    {
        this._computeRole();
        this._computeLabel();
        this._computeIgnoredState();
        this._computeWarnings();
    }

    _computeRole()
    {
        if ((this._properties.ignored || this._properties.ignoredByDefault) && !this._properties.role)
            this._computedRole = WI.UIString("Ignored <%s>").format(this._domNode.nodeNameInCorrectCase());
        else if (this._properties.role === "generic")
            this._computedRole = WI.UIString("Generic <%s>").format(this._domNode.nodeNameInCorrectCase());
        else if (this._properties.role)
            this._computedRole = this._properties.role;
    }

    _computeLabel()
    {
        let label = this._properties.label;
        
        if (!label && this._properties.childNodeIds && this._properties.childNodeIds.length > 0) {
            label = this._extractTextContent(this._properties.childNodeIds);
        }
        
        this._computedLabel = label;
    }


    _computeIgnoredState()
    {
        this._isIgnored = !!(this._properties.ignored || this._properties.ignoredByDefault);
    }

    _computeWarnings()
    {
        this._warningMessages = [];
        
        const role = this._properties.role;
        const label = this._computedLabel;
        
        if (role === "image" && !label) {
            this._warningMessages.push(WI.UIString("Image elements should have a label for accessibility"));
        } else if (role === "link" && !label) {
            this._warningMessages.push(WI.UIString("Link elements should have a label for accessibility"));
        } else if (role === "textbox" && !label) {
            this._warningMessages.push(WI.UIString("Textbox elements should have a label for accessibility"));
        }
    }

    shouldBeDisplayed()
    {
        return this._computedRole !== null;
    }

    _extractTextContent(childNodeIds)
    {
        if (!childNodeIds || !childNodeIds.length)
            return null;

        let firstChildNodeId = childNodeIds[0];
        let childDOMNode = WI.domManager.nodeForId(firstChildNodeId);
        if (childDOMNode && childDOMNode.nodeType() === Node.TEXT_NODE)
            return childDOMNode.nodeValue();

        return null;
    }
};