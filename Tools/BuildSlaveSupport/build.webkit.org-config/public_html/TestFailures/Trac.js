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

function Trac(baseURL) {
    this.baseURL = baseURL;
    this._cache = {};
}

Trac.prototype = {
    changesetURL: function(revision) {
        return this.baseURL + 'changeset/' + revision;
    },

    getCommitDataForRevisionRange: function(path, startRevision, endRevision, callback) {
        // FIXME: We could try to be smarter and cache individual commits, but in practice we just
        // get called with the same parameters over and over.
        var cacheKey = 'getCommitDataForRevisionRange.' + [path, startRevision, endRevision].join('.');
        if (cacheKey in this._cache) {
            callback(this._cache[cacheKey]);
            return;
        }

        var callbacksCacheKey = 'callbacks.' + cacheKey;
        if (callbacksCacheKey in this._cache) {
            this._cache[callbacksCacheKey].push(callback);
            return;
        }

        this._cache[callbacksCacheKey] = [callback];

        var self = this;

        function cacheResultsAndCallCallbacks(commits) {
            self._cache[cacheKey] = commits;

            var callbacks = self._cache[callbacksCacheKey];
            delete self._cache[callbacksCacheKey];

            callbacks.forEach(function(callback) {
                callback(commits);
            });
        }

        getResource(self.logURL('trunk', startRevision, endRevision, true, true), function(xhr) {
            var commits = Array.prototype.map.call(xhr.responseXML.getElementsByTagName('item'), function(item) {
                var title = item.getElementsByTagName('title')[0].textContent;
                var revision = parseInt(/^Revision (\d+):/.exec(title)[1], 10);

                var container = document.createElement('div');
                container.innerHTML = item.getElementsByTagName('description')[0].textContent;
                var listItems = container.querySelectorAll('li');
                var files = [];
                for (var i = 0; i < listItems.length; ++i) {
                    var match = /^([^:]+)/.exec(listItems[i].textContent);
                    if (!match)
                        continue;
                    files.push(match[1]);
                }

                return {
                    revision: revision,
                    title: title,
                    modifiedFiles: files,
                };
            });

            cacheResultsAndCallCallbacks(commits);
        },
        function(xhr) {
            cacheResultsAndCallCallbacks([]);
        });
    },

    logURL: function(path, startRevision, endRevision, showFullCommitLogs, formatAsRSS) {
        var queryParameters = {
            rev: endRevision,
            stop_rev: startRevision,
        };
        if (showFullCommitLogs)
            queryParameters.verbose = 'on';
        if (formatAsRSS)
            queryParameters.format = 'rss';
        return addQueryParametersToURL(this.baseURL + 'log/' + path, queryParameters);
    },
};
