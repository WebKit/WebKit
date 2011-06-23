/*
 * Copyright (C) 2011 Apple Inc. All rights reserved.
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

function Bugzilla(baseURL) {
    this.baseURL = baseURL;
    this._cache = {};
}

Bugzilla.prototype = {
    quickSearch: function(query, callback) {
        var cacheKey = 'quickSearch_' + query;
        if (cacheKey in this._cache) {
            callback(this._cache[cacheKey]);
            return;
        }

        var callbacksCacheKey = 'quickSearchCallbacks_' + query;
        if (callbacksCacheKey in this._cache) {
            this._cache[callbacksCacheKey].push(callback);
            return;
        }

        this._cache[callbacksCacheKey] = [callback];

        var queryParameters = {
            ctype: 'rss',
            order: 'bugs.bug_id desc',
            quicksearch: query,
        };

        var self = this;
        getResource(addQueryParametersToURL(this.baseURL + 'buglist.cgi', queryParameters), function(xhr) {
            var entries = xhr.responseXML.getElementsByTagName('entry');
            var results = Array.prototype.map.call(entries, function(entry) {
                var container = document.createElement('div');
                container.innerHTML = entry.getElementsByTagName('summary')[0].textContent;
                var statusRow = container.querySelector('tr.bz_feed_bug_status');
                return {
                    title: entry.getElementsByTagName('title')[0].textContent,
                    url: entry.getElementsByTagName('id')[0].textContent,
                    status: statusRow.cells[1].textContent,
                };
            });

            self._cache[cacheKey] = results;

            var callbacks = self._cache[callbacksCacheKey];
            delete self._cache[callbacksCacheKey];

            callbacks.forEach(function(callback) {
                callback(results);
            });
        });
    },
};

Bugzilla.isOpenStatus = function(status) {
    const openStatuses = {
        UNCONFIRMED: true,
        NEW: true,
        ASSIGNED: true,
        REOPENED: true,

    };
    return status in openStatuses;
};
