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

MockCommits = function(url)
{
    BaseObject.call(this);
    this.heads = {'main': {'identifier': '33022@main'}};
    this.commits = {
        '33021@main': {'identifier': '33021@main'},
        '33022@main': this.heads['main'],
    }
};

BaseObject.addConstructorFunctions(MockCommits);

MockCommits.UpdateInterval = 45000; // 45 seconds

MockCommits.prototype = {
    constructor: MockCommits,
    __proto__: BaseObject.prototype,

    _update: function() {},
    startPeriodicUpdates: function() {},
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
    fetch: function(branch, count) {},
    urlFor: function(identifier) {
        return `${this.url}/${identifier}`;
    }
};
