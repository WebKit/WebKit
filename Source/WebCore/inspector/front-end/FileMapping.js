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
    /** @type {Array.<WebInspector.FileMapping.Entry>} */
    this._entries = [];
    this._loadFromSettings();
}

WebInspector.FileMapping.prototype = {
    /**
     * @param {WebInspector.FileMapping.Entry} entry
     * @param {string} url
     * @return {boolean}
     */
    _entryMatchesURL: function(entry, url)
    {
        return url.startsWith(entry.urlPrefix);
    },
    
    /**
     * @param {WebInspector.FileMapping.Entry} entry
     * @return {?string}
     */
    _entryURIPrefix: function(entry)
    {
        return this._fileSystemMapping.uriPrefixForPathPrefix(entry.pathPrefix);
    },
    
    /**
     * @param {string} url
     * @return {boolean}
     */
    hasMappingForURL: function(url)
    {
        return !!this._innerURIForURL(url);
    },
    
    /**
     * @param {string} url
     * @return {?string}
     */
    _innerURIForURL: function(url)
    {
        for (var i = 0; i < this._entries.length; ++i) {
            var entry = this._entries[i];
            var uriPrefix = this._entryURIPrefix(entry);
            if (uriPrefix && this._entryMatchesURL(entry, url))
                return uriPrefix + url.substring(entry.urlPrefix.length);
        }
        return null;
    },
    
    /**
     * @param {string} url
     * @return {string}
     */
    uriForURL: function(url)
    {
        return this._innerURIForURL(url) || WebInspector.SimpleWorkspaceProvider.uriForURL(url, WebInspector.projectTypes.Network);
    },
    
    /**
     * @param {string} path
     * @return {string}
     */
    urlForPath: function(path)
    {
        for (var i = 0; i < this._entries.length; ++i) {
            var entry = this._entries[i];
            if (path.startsWith(entry.pathPrefix))
                return entry.urlPrefix + path.substring(entry.pathPrefix.length);
        }
        return "";
    },

    /**
     * @return {Array.<WebInspector.FileMapping.Entry>}
     */
    mappingEntries: function()
    {
        return this._entries.slice();
    },

    /**
     * @param {Array.<WebInspector.FileMapping.Entry>} mappingEntries
     */
    setMappingEntries: function(mappingEntries)
    {
        this._entries = mappingEntries;
        this._mappingEntriesSetting.set(mappingEntries);
    },

    _loadFromSettings: function()
    {
        var savedEntries = this._mappingEntriesSetting.get();
        this._entries = [];
        for (var i = 0; i < savedEntries.length; ++i) {
            var entry = new WebInspector.FileMapping.Entry(savedEntries[i].urlPrefix, savedEntries[i].pathPrefix);
            this._entries.push(entry);
        }
    },

    __proto__: WebInspector.Object.prototype
}

/**
 * @constructor
 * @param {string} urlPrefix
 * @param {string} pathPrefix
 */
WebInspector.FileMapping.Entry = function(urlPrefix, pathPrefix)
{
    this.urlPrefix = urlPrefix;
    this.pathPrefix = pathPrefix;
}

/**
 * @type {?WebInspector.FileMapping}
 */
WebInspector.fileMapping = null;
