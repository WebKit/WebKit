/*
 * Copyright (C) 2022 Apple Inc. All rights reserved.
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

Commits = function(url)
{
    BaseObject.call(this);

    console.assert(url);
    this.url = url;
    this.heads = {'main': null};
    this.commits = {}
};

BaseObject.addConstructorFunctions(Commits);

Commits.UpdateInterval = 45000; // 45 seconds

Commits.prototype = {
    constructor: Commits,
    __proto__: BaseObject.prototype,

    _update: function() {
        var self = this
        Object.entries(self.heads).forEach(function(branch){
            JSON.load(self.url + '/' + branch[0] + '/json', function(data) {
                self.heads[branch[0]] = data;
                self.commits[data.identifier] = data;
            });
        });
    },
    startPeriodicUpdates: function() {
        this._update()
        this.updateTimer = setInterval(this._update.bind(this), Commits.UpdateInterval);
    },
    branchPosition: function(identifier) {
        var split = identifier.split(/\.|@/);
        if (split.length === 2)
            return parseFloat(split[0]);
        if (split.length === 3)
            return parseFloat(split[1]);
        return null
    },
    lastNIdentifiers(branch, count) {
        if (!this.heads[branch])
            return [];

        var split = this.heads[branch].identifier.split(/\.|@/);
        var result = [];
        for(; count--;) {
            var identifier = '';
            if (split.length === 2)
                identifier = `${parseFloat(split[0]) - count}@${branch}`;
            else if (split.length === 3)
                identifier = `${split[0]}.${parseFloat(split[1]) - count}@${branch}`;

            result.push(identifier);
        }
        return result.reverse();
    },
    fetch: function(branch, count) {
        var self = this;
        this.lastNIdentifiers(branch, count).forEach((identifier) => {
            if (self.commits[identifier] !== undefined)
                return;
            self.commits[identifier] = null;

            JSON.load(self.url + '/' + identifier + '/json', function(data) {
                self.commits[data.identifier] = data;
            });
        });
    },
    urlFor: function(identifier) {
        return `${this.url}/${identifier}`;
    }
};
