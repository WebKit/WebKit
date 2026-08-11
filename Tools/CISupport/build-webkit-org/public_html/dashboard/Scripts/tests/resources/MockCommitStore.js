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

MockCommitStore = function(url)
{
    CommitStore.call(this);

    const identifier = '33022@main';
    const hash = '4c24d7e5e167b694c4fe729969548987bee8d953';
    const commit = {
        'identifier': identifier,
        'hash': hash,
        'timestamp': 1234457296,
        'order': 0,
    };

    this.commitsByBranch = {'main': [commit]};
    this.indexForCommit = {identifier: 0, hash: 0};
    this.commitsByRef = {identifier: commit, hash: commit};
    this.useIdentifiers = true;
};

BaseObject.addConstructorFunctions(MockCommitStore);

MockCommitStore.prototype = {
    constructor: MockCommitStore,
    __proto__: CommitStore.prototype,

    urlFor: function(ref) {
        return `https://commits.webkit.org/${ref}`;
    },
    fetch: function(base, count) {},
};
