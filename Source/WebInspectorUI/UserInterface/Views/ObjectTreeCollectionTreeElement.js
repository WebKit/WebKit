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

WebInspector.ObjectTreeCollectionTreeElement = function(remoteObject)
{
    console.assert(remoteObject instanceof WebInspector.RemoteObject);

    this._remoteObject = remoteObject;
    this._requestingEntries = false;
    this._trackingEntries = false;

    TreeElement.call(this, "<entries>", null, false);
    this.toggleOnClick = true;
    this.selectable = false;
    this.hasChildren = true;
    this.expand();

    // FIXME: When a parent TreeElement is collapsed, we do not get a chance
    // to releaseWeakCollectionEntries. We should.
};

WebInspector.ObjectTreeCollectionTreeElement.propertyDescriptorForEntry = function(name, value)
{
    var descriptor = {name: name, value: value, enumerable: true, writable: false};
    return new WebInspector.PropertyDescriptor(descriptor, true, false, false);
}

WebInspector.ObjectTreeCollectionTreeElement.prototype = {
    constructor: WebInspector.ObjectTreeCollectionTreeElement,
    __proto__: TreeElement.prototype,

    // Public

    get remoteObject()
    {
        return this._remoteObject;
    },

    // Protected

    onexpand: function()
    {
        if (this.children.length && !this.shouldRefreshChildren)
            return;

        if (this._requestingEntries)
            return;

        this._requestingEntries = true;

        function callback(entries) {
            this._requestingEntries = false;

            this.removeChildren();

            if (!entries || !entries.length) {
                this.appendChild(new WebInspector.ObjectTreeEmptyCollectionTreeElement);
                return;
            }

            this._trackWeakEntries();

            for (var i = 0; i < entries.length; ++i) {
                var entry = entries[i];
                if (entry.key)
                    this.appendChild(new WebInspector.ObjectTreeCollectionEntryTreeElement(entry, i));
                else {
                    var propertyDescriptor = WebInspector.ObjectTreeCollectionTreeElement.propertyDescriptorForEntry("" + i, entry.value);
                    this.appendChild(new WebInspector.ObjectTreePropertyTreeElement(propertyDescriptor));
                }
            }
        }

        this._remoteObject.getCollectionEntries(0, 100, callback.bind(this));
    },

    oncollapse: function()
    {
        this._untrackWeakEntries();
    },

    ondetach: function()
    {
        this._untrackWeakEntries();
    },

    // Private.

    _trackWeakEntries: function()
    {
        if (!this._remoteObject.isWeakCollection())
            return;

        if (this._trackingEntries)
            return;

        this._trackingEntries = true;

        WebInspector.logManager.addEventListener(WebInspector.LogManager.Event.Cleared, this._untrackWeakEntries, this);
        WebInspector.logManager.addEventListener(WebInspector.LogManager.Event.ActiveLogCleared, this._untrackWeakEntries, this);
        WebInspector.logManager.addEventListener(WebInspector.LogManager.Event.SessionStarted, this._untrackWeakEntries, this);
    },

    _untrackWeakEntries: function()
    {
        if (!this._remoteObject.isWeakCollection())
            return;

        if (!this._trackingEntries)
            return;

        this._trackingEntries = false;

        this._remoteObject.releaseWeakCollectionEntries();

        WebInspector.logManager.removeEventListener(WebInspector.LogManager.Event.Cleared, this._untrackWeakEntries, this);
        WebInspector.logManager.removeEventListener(WebInspector.LogManager.Event.ActiveLogCleared, this._untrackWeakEntries, this);
        WebInspector.logManager.removeEventListener(WebInspector.LogManager.Event.SessionStarted, this._untrackWeakEntries, this);

        this.removeChildren();

        if (this.expanded)
            this.collapse();
    },
};

WebInspector.ObjectTreeCollectionEntryTreeElement = function(entry, index)
{
    console.assert(entry instanceof WebInspector.CollectionEntry);
    console.assert(entry.key instanceof WebInspector.RemoteObject);
    console.assert(entry.value instanceof WebInspector.RemoteObject);

    this._name = "" + index;
    this._key = entry.key;
    this._value = entry.value;

    TreeElement.call(this, "", null, false);
    this.toggleOnClick = true;
    this.selectable = false;
    this.hasChildren = true;
}

WebInspector.ObjectTreeCollectionEntryTreeElement.prototype = {
    constructor: WebInspector.ObjectTreeCollectionEntryTreeElement,
    __proto__: TreeElement.prototype,

    // Protected

    onpopulate: function()
    {
        if (this.children.length && !this.shouldRefreshChildren)
            return;

        var keyPropertyDescriptor = WebInspector.ObjectTreeCollectionTreeElement.propertyDescriptorForEntry("key", this._key);
        this.appendChild(new WebInspector.ObjectTreePropertyTreeElement(keyPropertyDescriptor));

        var valuePropertyDescriptor = WebInspector.ObjectTreeCollectionTreeElement.propertyDescriptorForEntry("value", this._value);
        this.appendChild(new WebInspector.ObjectTreePropertyTreeElement(valuePropertyDescriptor));
    },

    onattach: function()
    {
        var nameElement = document.createElement("span");
        nameElement.className = "name";
        nameElement.textContent = "" + this._name;

        var separatorElement = document.createElement("span");
        separatorElement.className = "separator";
        separatorElement.textContent = ": ";

        var valueElement = document.createElement("span");
        valueElement.className = "value";
        valueElement.textContent = "{" + this._key.description + " => " + this._value.description + "}";

        this.listItemElement.classList.add("object-tree-property");

        this.listItemElement.removeChildren();
        this.listItemElement.appendChild(nameElement);
        this.listItemElement.appendChild(separatorElement);
        this.listItemElement.appendChild(valueElement);
    }
};

WebInspector.ObjectTreeEmptyCollectionTreeElement = function()
{
    TreeElement.call(this, WebInspector.UIString("Empty Collection"), null, false);
    this.selectable = false;
}

WebInspector.ObjectTreeEmptyCollectionTreeElement.prototype = {
    constructor: WebInspector.ObjectTreeEmptyCollectionTreeElement,
    __proto__: TreeElement.prototype
};
