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

WI.DOMBreakpoint = class DOMBreakpoint extends WI.Breakpoint
{
    constructor(domNodeOrInfo, type, {disabled} = {})
    {
        console.assert(domNodeOrInfo instanceof WI.DOMNode || typeof domNodeOrInfo === "object", domNodeOrInfo);
        console.assert(Object.values(WI.DOMBreakpoint.Type).includes(type), type);

        super({disabled});

        if (domNodeOrInfo instanceof WI.DOMNode) {
            this._domNodeIdentifier = domNodeOrInfo.id;
            this._path = domNodeOrInfo.path();
            console.assert(WI.networkManager.mainFrame);
            this._url = WI.networkManager.mainFrame.url;
        } else if (domNodeOrInfo && typeof domNodeOrInfo === "object") {
            this._domNodeIdentifier = null;
            this._path = domNodeOrInfo.path;
            this._url = domNodeOrInfo.url;
        }

        this._type = type;
    }

    // Static

    static displayNameForType(type)
    {
        console.assert(Object.values(WI.DOMBreakpoint.Type).includes(type), type);

        switch (type) {
        case WI.DOMBreakpoint.Type.SubtreeModified:
            return WI.UIString("Subtree Modified", "Subtree Modified @ DOM Breakpoint", "A submenu item of 'Break On' that breaks (pauses) before child DOM node is modified");

        case WI.DOMBreakpoint.Type.AttributeModified:
            return WI.UIString("Attribute Modified", "Attribute Modified @ DOM Breakpoint", "A submenu item of 'Break On' that breaks (pauses) before DOM attribute is modified");

        case WI.DOMBreakpoint.Type.NodeRemoved:
            return WI.UIString("Node Removed", "Node Removed @ DOM Breakpoint", "A submenu item of 'Break On' that breaks (pauses) before DOM node is removed");
        }

        console.error();
        return WI.UIString("DOM");
    }

    static fromJSON(json)
    {
        return new WI.DOMBreakpoint(json, json.type, {
            disabled: json.disabled,
        });
    }

    // Public

    get type() { return this._type; }
    get url() { return this._url; }
    get path() { return this._path; }

    get displayName()
    {
        return WI.DOMBreakpoint.displayNameForType(this._type);
    }

    get domNodeIdentifier()
    {
        return this._domNodeIdentifier;
    }

    set domNodeIdentifier(nodeIdentifier)
    {
        if (this._domNodeIdentifier === nodeIdentifier)
            return;

        let data = {};
        if (!nodeIdentifier)
            data.oldNodeIdentifier = this._domNodeIdentifier;

        this._domNodeIdentifier = nodeIdentifier;

        this.dispatchEventToListeners(WI.DOMBreakpoint.Event.DOMNodeChanged, data);
    }

    remove()
    {
        super.remove();

        WI.domDebuggerManager.removeDOMBreakpoint(this);
    }

    saveIdentityToCookie(cookie)
    {
        cookie["dom-breakpoint-url"] = this._url;
        cookie["dom-breakpoint-path"] = this._path;
        cookie["dom-breakpoint-type"] = this._type;
    }

    toJSON(key)
    {
        let json = super.toJSON(key);
        json.url = this._url;
        json.path = this._path;
        json.type = this._type;
        if (key === WI.ObjectStore.toJSONSymbol)
            json[WI.objectStores.domBreakpoints.keyPath] = this._url + ":" + this._path + ":" + this._type;
        return json;
    }
};

WI.DOMBreakpoint.Type = {
    SubtreeModified: "subtree-modified",
    AttributeModified: "attribute-modified",
    NodeRemoved: "node-removed",
};

WI.DOMBreakpoint.Event = {
    DOMNodeChanged: "dom-breakpoint-dom-node-changed",
};

WI.DOMBreakpoint.ReferencePage = WI.ReferencePage.DOMBreakpoints;
