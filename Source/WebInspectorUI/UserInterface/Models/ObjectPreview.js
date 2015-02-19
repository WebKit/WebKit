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

WebInspector.ObjectPreview = function(type, subtype, description, lossless, overflow, properties, entries)
{
    WebInspector.Object.call(this);

    console.assert(type);
    console.assert(typeof lossless === "boolean");
    console.assert(!properties || !properties.length || properties[0] instanceof WebInspector.PropertyPreview);
    console.assert(!entries || !entries.length || entries[0] instanceof WebInspector.CollectionEntryPreview);

    this._type = type;
    this._subtype = subtype;
    this._description = description || "";
    this._lossless = lossless;
    this._overflow = overflow || false;

    this._properties = properties || null;
    this._entries = entries || null;
};

// Runtime.ObjectPreview.
WebInspector.ObjectPreview.fromPayload = function(payload)
{
    if (payload.properties)
        payload.properties = payload.properties.map(function(property) { return WebInspector.PropertyPreview.fromPayload(property); });
    if (payload.entries)
        payload.entries = payload.entries.map(function(entry) { return WebInspector.CollectionEntryPreview.fromPayload(entry); });

    return new WebInspector.ObjectPreview(payload.type, payload.subtype, payload.description, payload.lossless, payload.overflow, payload.properties, payload.entries);
};

WebInspector.ObjectPreview.prototype = {
    constructor: WebInspector.ObjectPreview,
    __proto__: WebInspector.Object.prototype,

    // Public

    get type()
    {
        return this._type;
    },

    get subtype()
    {
        return this._subtype;
    },

    get description()
    {
        return this._description;
    },

    get lossless()
    {
        return this._lossless;
    },

    get overflow()
    {
        return this._overflow;
    },

    get propertyPreviews()
    {
        return this._properties;
    },

    get collectionEntryPreviews()
    {
        return this._entries;
    }
};
