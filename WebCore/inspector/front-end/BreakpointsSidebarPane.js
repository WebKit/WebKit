/*
 * Copyright (C) 2008 Apple Inc. All Rights Reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

WebInspector.BreakpointsSidebarPane = function(title)
{
    WebInspector.SidebarPane.call(this, title);

    this.listElement = document.createElement("ol");
    this.listElement.className = "breakpoint-list";

    this.emptyElement = document.createElement("div");
    this.emptyElement.className = "info";
    this.emptyElement.textContent = WebInspector.UIString("No Breakpoints");

    this.bodyElement.appendChild(this.emptyElement);
}

WebInspector.BreakpointsSidebarPane.prototype = {
    reset: function()
    {
        this.listElement.removeChildren();
        if (this.listElement.parentElement) {
            this.bodyElement.removeChild(this.listElement);
            this.bodyElement.appendChild(this.emptyElement);
        }
    },

    addBreakpoint: function(breakpointItem)
    {
        breakpointItem.addEventListener("removed", this._breakpointRemoved, this);

        var element = breakpointItem.element();
        element._breakpointItem = breakpointItem;

        var currentElement = this.listElement.firstChild;
        while (currentElement) {
             if (currentElement._breakpointItem.compareTo(element._breakpointItem) > 0) {
                this.listElement.insertBefore(element, currentElement);
                break;
            }
            currentElement = currentElement.nextSibling;
        }
        if (!currentElement)
            this.listElement.appendChild(element);

        element.addEventListener("contextmenu", this._contextMenuEventFired.bind(this, breakpointItem), true);

        if (this.emptyElement.parentElement) {
            this.bodyElement.removeChild(this.emptyElement);
            this.bodyElement.appendChild(this.listElement);
        }
    },

    _breakpointRemoved: function(event)
    {
        this.listElement.removeChild(event.target.element());
        if (!this.listElement.firstChild) {
            this.bodyElement.removeChild(this.listElement);
            this.bodyElement.appendChild(this.emptyElement);
        }
    },

    _contextMenuEventFired: function(breakpointItem, event)
    {
        var contextMenu = new WebInspector.ContextMenu();
        contextMenu.appendItem(WebInspector.UIString("Remove Breakpoint"), breakpointItem.remove.bind(breakpointItem));
        contextMenu.show(event);
    }
}

WebInspector.BreakpointsSidebarPane.prototype.__proto__ = WebInspector.SidebarPane.prototype;

WebInspector.BreakpointItem = function(breakpoint)
{
    this._breakpoint = breakpoint;

    this._element = document.createElement("li");

    var checkboxElement = document.createElement("input");
    checkboxElement.className = "checkbox-elem";
    checkboxElement.type = "checkbox";
    checkboxElement.checked = this._breakpoint.enabled;
    checkboxElement.addEventListener("click", this._checkboxClicked.bind(this), false);
    this._element.appendChild(checkboxElement);

    this._breakpoint.addEventListener("enable-changed", this._enableChanged, this);
    this._breakpoint.addEventListener("removed", this._removed, this);
}

WebInspector.BreakpointItem.prototype = {
    element: function()
    {
        return this._element;
    },

    remove: function()
    {
        this._breakpoint.remove();
    },

    _checkboxClicked: function(event)
    {
        this._breakpoint.enabled = !this._breakpoint.enabled;

        // without this, we'd switch to the source of the clicked breakpoint
        event.stopPropagation();
    },

    _enableChanged: function()
    {
        var checkbox = this._element.firstChild;
        checkbox.checked = this._breakpoint.enabled;
    },

    _removed: function()
    {
        this.dispatchEventToListeners("removed");
    }
}

WebInspector.BreakpointItem.prototype.__proto__ = WebInspector.Object.prototype;

WebInspector.JSBreakpointItem = function(breakpoint)
{
    WebInspector.BreakpointItem.call(this, breakpoint);

    this._element.addEventListener("click", this._breakpointClicked.bind(this), false);

    var displayName = this._breakpoint.url ? WebInspector.displayNameForURL(this._breakpoint.url) : WebInspector.UIString("(program)");
    var labelElement = document.createTextNode(displayName + ":" + this._breakpoint.line);
    this._element.appendChild(labelElement);

    var sourceTextElement = document.createElement("div");
    sourceTextElement.textContent = this._breakpoint.sourceText;
    sourceTextElement.className = "source-text monospace";
    this._element.appendChild(sourceTextElement);

    this._breakpoint.addEventListener("text-changed", this._textChanged, this);
}

WebInspector.JSBreakpointItem.prototype = {
    compareTo: function(other)
    {
        if (this._breakpoint.url != other._breakpoint.url)
            return this._breakpoint.url < other._breakpoint.url ? -1 : 1;
        if (this._breakpoint.line != other._breakpoint.line)
            return this._breakpoint.line < other._breakpoint.line ? -1 : 1;
        return 0;
    },

    _breakpointClicked: function()
    {
        WebInspector.panels.scripts.showSourceLine(this._breakpoint.url, this._breakpoint.line);
    },

    _textChanged: function()
    {
        var sourceTextElement = this._element.firstChild.nextSibling.nextSibling;
        sourceTextElement.textContent = this._breakpoint.sourceText;
    }
}

WebInspector.JSBreakpointItem.prototype.__proto__ = WebInspector.BreakpointItem.prototype;

WebInspector.DOMBreakpointItem = function(breakpoint)
{
    WebInspector.BreakpointItem.call(this, breakpoint);

    var node = WebInspector.domAgent.nodeForId(this._breakpoint.nodeId);
    var link = WebInspector.panels.elements.linkifyNodeReference(node);
    this._element.appendChild(link);

    var type = WebInspector.DOMBreakpoint.labelForType(this._breakpoint.type);
    var typeElement = document.createTextNode(" - " + type);
    this._element.appendChild(typeElement);
}

WebInspector.DOMBreakpointItem.prototype = {
    compareTo: function(other)
    {
        if (this._breakpoint.type != other._breakpoint.type)
            return this._breakpoint.type < other._breakpoint.type ? -1 : 1;
        return 0;
    }
}

WebInspector.DOMBreakpointItem.prototype.__proto__ = WebInspector.BreakpointItem.prototype;
