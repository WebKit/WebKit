/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/**
 * @constructor
 * @param {WebInspector.FileSystemMapping} fileSystemMapping
 */
WebInspector.FileMapping = function(fileSystemMapping)
{
    this._fileSystemMapping = fileSystemMapping;
    this._mappingEntriesSetting = WebInspector.settings.createSetting("fileMappingEntries", []);
    /** @type {Array.<WebInspector.FileMapping.MappingEntry>} */
    this._mappingEntries = [];
    this._deserialize(/** @type {Array.<WebInspector.FileMapping.MappingEntry>} */ (this._mappingEntriesSetting.get()));
}

/** @typedef {{urlPrefix: string, pathPrefix: string}} */
WebInspector.FileMapping.SerializedMappingEntry;

WebInspector.FileMapping.prototype = {
    /**
     * @param {string} url
     * @return {boolean}
     */
    hasMappingForURL: function(url)
    {
        for (var i = 0; i < this._mappingEntries.length; ++i) {
            if (this._mappingEntries[i].matchesURL(url))
                return true;
        }
        return false;
    },
    
    /**
     * @param {string} url
     * @return {string}
     */
    uriForURL: function(url)
    {
        for (var i = 0; i < this._mappingEntries.length; ++i) {
            var uri = this._mappingEntries[i].uriForURL(url);
            if (typeof uri === "string")
                return uri;
        }
        // FIXME: FileMapping should be network project aware. It should return correct uri for network project uiSourceCodes.
        return url;
    },
    
    /**
     * @param {string} uri
     * @return {?string}
     */
    urlForURI: function(uri)
    {
        for (var i = 0; i < this._mappingEntries.length; ++i) {
            if (this._mappingEntries[i].matchesURI(uri))
                return this._mappingEntries[i].urlForURI(uri);
        }
        return null;
    },

    /**
     * @param {Array.<WebInspector.FileMapping.SerializedMappingEntry>} serializedMappingEntries
     */
    setMappings: function(serializedMappingEntries)
    {
        this._deserialize(serializedMappingEntries);
        this._serialize();
    },

    /**
     * @return {Array.<WebInspector.FileMapping.MappingEntry>}
     */
    mappings: function()
    {
        return this._mappingEntries.slice();
    },

    /**
     * @param {Array.<WebInspector.FileMapping.SerializedMappingEntry>} serializedMappingEntries
     */
    _deserialize: function(serializedMappingEntries)
    {
        this._mappingEntries = [];
        for (var i = 0; i < serializedMappingEntries.length; ++i)
            this._mappingEntries.push(WebInspector.FileMapping.MappingEntry.deserialize(serializedMappingEntries[i], this._fileSystemMapping));
    },

    _serialize: function()
    {
        var savedEntries = [];
        for (var i = 0; i < this._mappingEntries.length; ++i)
            savedEntries.push(this._mappingEntries[i].serialize());
        this._mappingEntriesSetting.set(savedEntries);
    },

    __proto__: WebInspector.Object.prototype
}

/**
 * @constructor
 * @param {string} urlPrefix
 * @param {string} pathPrefix
 * @param {?string} uriPrefix
 */
WebInspector.FileMapping.MappingEntry = function(urlPrefix, pathPrefix, uriPrefix)
{
    this._urlPrefix = urlPrefix;
    this._pathPrefix = pathPrefix;
    this._uriPrefix = uriPrefix;
}

/**
 * @param {WebInspector.FileMapping.SerializedMappingEntry} serializedMappingEntry
 * @param {WebInspector.FileSystemMapping} fileSystemMapping
 */
WebInspector.FileMapping.MappingEntry.deserialize = function(serializedMappingEntry, fileSystemMapping)
{
    var uriPrefix = fileSystemMapping.uriForPath(serializedMappingEntry.pathPrefix);
    return new WebInspector.FileMapping.MappingEntry(serializedMappingEntry.urlPrefix, serializedMappingEntry.pathPrefix, uriPrefix);
}

WebInspector.FileMapping.MappingEntry.prototype = {
    /**
     * @param {string} url
     * @return {boolean}
     */
    matchesURL: function(url)
    {
        return url.indexOf(this._urlPrefix) === 0;
    },
    
    /**
     * @param {string} uri
     * @return {boolean}
     */
    matchesURI: function(uri)
    {
        if (!this._uriPrefix)
            return false;
        return uri.indexOf(this._uriPrefix) === 0;
    },
    
    /**
     * @param {string} url
     * @return {?string}
     */
    uriForURL: function(url)
    {
        if (this._uriPrefix && this.matchesURL(url))
            return this._uriPrefix + url.substring(this._urlPrefix.length);
        return null;
    },
    
    /**
     * @param {string} uri
     * @return {?string}
     */
    urlForURI: function(uri)
    {
        if (this.matchesURI(uri))
            return this._urlPrefix + uri.substring(this._uriPrefix.length);
        return null;
    },
    
    /**
     * @return {Object}
     */
    serialize: function()
    {
        return { urlPrefix: this._urlPrefix, pathPrefix: this._pathPrefix };
    },

    __proto__: WebInspector.Object.prototype
}

/**
 * @type {?WebInspector.FileMapping}
 */
WebInspector.fileMapping = null;
