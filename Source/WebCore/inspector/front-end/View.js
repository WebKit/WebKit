/*
 * Copyright (C) 2008 Apple Inc. All Rights Reserved.
 * Copyright (C) 2011 Google Inc. All Rights Reserved.
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

/**
 * @constructor
 * @extends {WebInspector.Object}
 */
WebInspector.View = function()
{
    this.element = document.createElement("div");
    this.element.addEventListener("DOMNodeInsertedIntoDocument", this._handleInsertedIntoDocument.bind(this), false);
    this.element.addEventListener("DOMNodeRemovedFromDocument", this._handleRemovedFromDocument.bind(this), false);
    this.element.__view = this;
    this._visible = false;
    this._children = [];
}

WebInspector.View.prototype = {
    get visible()
    {
        return this._visible;
    },

    set visible(x)
    {
        if (this._visible === x)
            return;

        if (x)
            this.show(this.element.parentElement);
        else
            this.hide();
    },

    wasShown: function()
    {
        this.restoreScrollPositions();
        this.onResize();
    },

    willHide: function()
    {
        this.storeScrollPositions();
    },

    willDetach: function()
    {
    },

    /**
     * @param {Element} parentElement
     */
    show: function(parentElement)
    {
        this._visible = true;
        this.element.addStyleClass("visible");
        if (parentElement && this.element.parentElement != parentElement)
            this.attach(parentElement);
        this.dispatchToSelfAndChildren("wasShown", true);
    },

    hide: function()
    {
        this.dispatchToSelfAndChildren("willHide", true);
        this.element.removeStyleClass("visible");
        this._visible = false;
    },

    /**
     * @param {Element} parentElement
     */
    attach: function(parentElement)
    {
        parentElement.appendChild(this.element);
    },

    detach: function()
    {
        this.dispatchToSelfAndChildren("willDetach", false);

        if (this.element.parentElement)
            this.element.parentElement.removeChild(this.element);
    },

    elementsToRestoreScrollPositionsFor: function()
    {
        return [this.element];
    },

    storeScrollPositions: function()
    {
        var elements = this.elementsToRestoreScrollPositionsFor();
        for (var i = 0; i < elements.length; ++i) {
            var container = elements[i];
            container._scrollTop = container.scrollTop;
            container._scrollLeft = container.scrollLeft;
        }
    },

    restoreScrollPositions: function()
    {
        var elements = this.elementsToRestoreScrollPositionsFor();
        for (var i = 0; i < elements.length; ++i) {
            var container = elements[i];
            if (container._scrollTop)
                container.scrollTop = container._scrollTop;
            if (container._scrollLeft)
                container.scrollLeft = container._scrollLeft;
        }
    },

    _addChildView: function(view)
    {
        this._children.push(view);
        view._parentView = this;
    },

    _removeChildView: function(view)
    {
        var childIndex = this._children.indexOf(view);
        if (childIndex < 0) {
            console.error("Attempt to remove non-child view");
            return;
        }

        this._children.splice(childIndex, 1);
        view._parentView = null;
    },

    onResize: function()
    {
    },

    canHighlightLine: function()
    {
        return false;
    },

    highlightLine: function(line)
    {
    },

    doResize: function()
    {
        this.dispatchToSelfAndChildren("onResize", true);
    },

    dispatchToSelfAndChildren: function(methodName, visibleOnly)
    {
        if (visibleOnly && !this.visible)
            return;
        if (typeof this[methodName] === "function")
            this[methodName].call(this);
        this.dispatchToChildren(methodName, visibleOnly);
    },

    dispatchToChildren: function(methodName, visibleOnly)
    {
        if (visibleOnly && !this.visible)
            return;
        for (var i = 0; i < this._children.length; ++i)
            this._children[i].dispatchToSelfAndChildren(methodName, visibleOnly);
    },

    _handleInsertedIntoDocument: function(event)
    {
        var parentElement = this.element.parentElement;
        while (parentElement && !parentElement.__view)
            parentElement = parentElement.parentElement;

        var parentView = parentElement ? parentElement.__view : WebInspector._rootView;
        parentView._addChildView(this);
        this.onInsertedIntoDocument();
    },

    onInsertedIntoDocument: function()
    {
    },

    _handleRemovedFromDocument: function(event)
    {
        if (this._parentView)
            this._parentView._removeChildView(this);
    },

    printViewHierarchy: function()
    {
        var lines = [];
        this._collectViewHierarchy("", lines);
        console.log(lines.join("\n"));
    },

    _collectViewHierarchy: function(prefix, lines)
    {
        lines.push(prefix + "[" + this.element.className + "]" + (this._children.length ? " {" : ""));

        for (var i = 0; i < this._children.length; ++i)
            this._children[i]._collectViewHierarchy(prefix + "    ", lines);

        if (this._children.length)
            lines.push(prefix + "}");
    }
}

WebInspector.View.prototype.__proto__ = WebInspector.Object.prototype;

WebInspector._rootView = new WebInspector.View();
