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

WebInspector.DOMBreakpointTreeController = class DOMBreakpointsTreeController extends WebInspector.Object
{
    constructor(treeOutline)
    {
        super();

        this._treeOutline = treeOutline;
        this._breakpointTreeElements = new Map;
        this._domNodeTreeElements = new Map;

        WebInspector.DOMBreakpoint.addEventListener(WebInspector.DOMBreakpoint.Event.ResolvedStateDidChange, this._domBreakpointResolvedStateDidChange, this);
        WebInspector.Frame.addEventListener(WebInspector.Frame.Event.MainResourceDidChange, this._mainResourceDidChange, this);

        WebInspector.domDebuggerManager.addEventListener(WebInspector.DOMDebuggerManager.Event.DOMBreakpointAdded, this._domBreakpointAdded, this);
        WebInspector.domDebuggerManager.addEventListener(WebInspector.DOMDebuggerManager.Event.DOMBreakpointRemoved, this._domBreakpointRemoved, this);
    }

    // Static

    static appendBreakpointContextMenuItems(contextMenu, domNode, allowEditing)
    {
        let subMenu = contextMenu.appendSubMenuItem(WebInspector.UIString("Break on…"));

        let breakpoints = WebInspector.domDebuggerManager.domBreakpointsForNode(domNode);
        let keyValuePairs = breakpoints.map((breakpoint) => [breakpoint.type, breakpoint]);
        let breakpointsByType = new Map(keyValuePairs);

        for (let type of Object.values(WebInspector.DOMBreakpoint.Type)) {
            let label = WebInspector.DOMBreakpointTreeElement.displayNameForType(type);
            let breakpoint = breakpointsByType.get(type);

            subMenu.appendCheckboxItem(label, function() {
                if (breakpoint)
                    WebInspector.domDebuggerManager.removeDOMBreakpoint(breakpoint);
                else
                    WebInspector.domDebuggerManager.addDOMBreakpoint(new WebInspector.DOMBreakpoint(domNode, type));
            }, !!breakpoint, false);
        }

        if (allowEditing) {
            contextMenu.appendSeparator();

            let shouldEnable = breakpoints.some((breakpoint) => breakpoint.disabled);
            let label = shouldEnable ? WebInspector.UIString("Enable Breakpoints") : WebInspector.UIString("Disable Breakpoints");
            contextMenu.appendItem(label, () => {
                breakpoints.forEach((breakpoint) => breakpoint.disabled = !shouldEnable);
            });

            contextMenu.appendItem(WebInspector.UIString("Delete Breakpoints"), function() {
                let breakpoints = WebInspector.domDebuggerManager.domBreakpointsForNode(domNode);
                for (let breakpoint of breakpoints)
                    WebInspector.domDebuggerManager.removeDOMBreakpoint(breakpoint);
            });
        }
    }

    // Public

    disconnect()
    {
        WebInspector.DOMBreakpoint.removeEventListener(null, null, this);
        WebInspector.Frame.removeEventListener(null, null, this);
        WebInspector.domDebuggerManager.removeEventListener(null, null, this);
    }

    // Private

    _addBreakpointTreeElement(breakpoint)
    {
        let nodeIdentifier = breakpoint.domNodeIdentifier;
        let parentTreeElement = this._domNodeTreeElements.get(nodeIdentifier);
        let shouldExpandParent = false;

        if (!parentTreeElement) {
            let domNode = WebInspector.domTreeManager.nodeForId(nodeIdentifier);
            console.assert(domNode, "Missing DOMNode for identifier", nodeIdentifier);

            parentTreeElement = new WebInspector.DOMNodeTreeElement(domNode);
            this._treeOutline.appendChild(parentTreeElement);
            this._domNodeTreeElements.set(nodeIdentifier, parentTreeElement);

            shouldExpandParent = true;
        }

        let treeElement = new WebInspector.DOMBreakpointTreeElement(breakpoint);
        parentTreeElement.appendChild(treeElement);

        if (shouldExpandParent)
            parentTreeElement.expand();

        this._breakpointTreeElements.set(breakpoint, treeElement);
    }

    _removeBreakpointTreeElement(breakpoint)
    {
        let breakpointTreeElement = this._breakpointTreeElements.get(breakpoint);
        if (!breakpointTreeElement)
            return;

        let domNodeTreeElement = breakpointTreeElement.parent;
        console.assert(domNodeTreeElement, "Missing parent DOM node tree element.");

        domNodeTreeElement.removeChild(breakpointTreeElement);
        this._breakpointTreeElements.delete(breakpoint);

        if (domNodeTreeElement.hasChildren)
            return;

        this._treeOutline.removeChild(domNodeTreeElement);
        this._domNodeTreeElements.delete(breakpoint.domNodeIdentifier);
    }

    _domBreakpointAdded(event)
    {
        let breakpoint = event.data.breakpoint;
        if (!breakpoint.domNodeIdentifier)
            return;

        this._addBreakpointTreeElement(breakpoint);
    }

    _domBreakpointRemoved(event)
    {
        this._removeBreakpointTreeElement(event.data.breakpoint);
    }

    _domBreakpointResolvedStateDidChange(event)
    {
        let breakpoint = event.target;
        if (breakpoint.domNodeIdentifier)
            this._addBreakpointTreeElement(breakpoint);
        else
            this._removeBreakpointTreeElement(breakpoint);
    }

    _mainResourceDidChange(event)
    {
        if (!event.target.isMainFrame())
            return;

        this._treeOutline.removeChildren();

        this._breakpointTreeElements.clear();
        this._domNodeTreeElements.clear();
    }
};
