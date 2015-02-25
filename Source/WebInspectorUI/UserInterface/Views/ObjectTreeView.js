/*
 * Copyright (C) 2015 Apple Inc. All rights reserved.
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

WebInspector.ObjectTreeView = function(object, mode, forceExpanding)
{
    WebInspector.Object.call(this);

    console.assert(object instanceof WebInspector.RemoteObject);

    this._object = object;
    this._mode = mode || WebInspector.ObjectTreeView.Mode.Properties;
    this._expanded = false;
    this._hasLosslessPreview = false;

    this._element = document.createElement("div");
    this._element.className = "object-tree";

    if (this._object.preview) {
        this._previewView = new WebInspector.ObjectPreviewView(this._object.preview);
        this._previewView.element.addEventListener("click", this._handlePreviewOrTitleElementClick.bind(this));
        this._element.appendChild(this._previewView.element);

        if (this._previewView.lossless && !forceExpanding) {
            this._hasLosslessPreview = true;
            this.element.classList.add("lossless-preview");
        }
    } else {
        this._titleElement = document.createElement("span");
        this._titleElement.className = "title";
        this._titleElement.textContent = this._object.description || "";
        this._titleElement.addEventListener("click", this._handlePreviewOrTitleElementClick.bind(this));
        this._element.appendChild(this._titleElement);
    }

    this._outlineElement = document.createElement("ol");
    this._outlineElement.className = "object-tree-outline";
    this._outline = new TreeOutline(this._outlineElement);
    this._element.appendChild(this._outlineElement);

    // FIXME: Support editable ObjectTrees.
};

WebInspector.ObjectTreeView.Mode = {
    Properties: Symbol("object-tree-properties"),
    API: Symbol("object-tree-api"),
};

WebInspector.ObjectTreeView.ComparePropertyDescriptors = function(propertyA, propertyB)
{
    var a = propertyA.name;
    var b = propertyB.name;

    // Put __proto__ at the bottom.
    if (a === "__proto__")
        return 1;
    if (b === "__proto__")
        return -1;

    // Put internal properties at the top.
    if (a.isInternalProperty && !b.isInternalProperty)
        return -1;
    if (b.isInternalProperty && !a.isInternalProperty)
        return 1;

    // if used elsewhere make sure to
    //  - convert a and b to strings (not needed here, properties are all strings)
    //  - check if a == b (not needed here, no two properties can be the same)

    var diff = 0;
    var chunk = /^\d+|^\D+/;
    var chunka, chunkb, anum, bnum;
    while (diff === 0) {
        if (!a && b)
            return -1;
        if (!b && a)
            return 1;
        chunka = a.match(chunk)[0];
        chunkb = b.match(chunk)[0];
        anum = !isNaN(chunka);
        bnum = !isNaN(chunkb);
        if (anum && !bnum)
            return -1;
        if (bnum && !anum)
            return 1;
        if (anum && bnum) {
            diff = chunka - chunkb;
            if (diff === 0 && chunka.length !== chunkb.length) {
                if (!+chunka && !+chunkb) // chunks are strings of all 0s (special case)
                    return chunka.length - chunkb.length;
                else
                    return chunkb.length - chunka.length;
            }
        } else if (chunka !== chunkb)
            return (chunka < chunkb) ? -1 : 1;
        a = a.substring(chunka.length);
        b = b.substring(chunkb.length);
    }
    return diff;
};

WebInspector.ObjectTreeView.prototype = {
    constructor: WebInspector.ObjectTreeView,
    __proto__: WebInspector.Object.prototype,

    // Public

    get object()
    {
        return this._object;
    },

    get element()
    {
        return this._element;
    },

    get expanded()
    {
        return this._expanded;
    },

    expand: function()
    {
        if (this._expanded)
            return;

        this._expanded = true;
        this._element.classList.add("expanded");

        if (this._previewView)
            this._previewView.showTitle();

        this.update();
    },

    collapse: function()
    {
        if (!this._expanded)
            return;

        this._expanded = false;
        this._element.classList.remove("expanded");

        if (this._previewView)
            this._previewView.showPreview();
    },

    // Protected

    update: function()
    {
        this._object.getOwnPropertyDescriptors(this._updateProperties.bind(this));
    },

    // Private

    _updateProperties: function(properties)
    {
        this._outline.removeChildren();

        if (!properties) {
            var errorMessageElement = document.createElement("div");
            errorMessageElement.className = "empty-message";
            errorMessageElement.textContent = WebInspector.UIString("Could not fetch properties. Object may no longer exist.");;
            this._outline.appendChild(new TreeElement(errorMessageElement, null, false));
            return;
        }

        properties.sort(WebInspector.ObjectTreeView.ComparePropertyDescriptors);

        // FIXME: Intialize component with "$n" instead of "obj".
        var rootPropertyPath = new WebInspector.PropertyPath(this._object, "obj");

        for (var propertyDescriptor of properties)
            this._outline.appendChild(new WebInspector.ObjectTreePropertyTreeElement(propertyDescriptor, rootPropertyPath, this._mode));

        // FIXME: Re-enable Collection Entries with new UI.
        // if (this._mode === WebInspector.ObjectTreeView.Mode.Properties) {
        //     if (this._object.isCollectionType())
        //         this._outline.appendChild(new WebInspector.ObjectTreeCollectionTreeElement(this._object));
        // }

        if (!this._outline.children.length) {
            var emptyMessageElement = document.createElement("div");
            emptyMessageElement.className = "empty-message";
            emptyMessageElement.textContent = WebInspector.UIString("No Properties");;
            this._outline.appendChild(new TreeElement(emptyMessageElement, null, false));
        }
    },

    _handlePreviewOrTitleElementClick: function(event)
    {
        if (this._hasLosslessPreview)
            return;

        if (!this._expanded)
            this.expand();
        else
            this.collapse();

        event.stopPropagation();
    }
};
