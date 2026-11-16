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

CommitStore = function()
{
    BaseObject.call(this);

    this.commitsByBranch = {'main': []};
    this.indexForCommit = {};
    this.commitsByRef = {};
    this.useIdentifiers = false;
};

BaseObject.addConstructorFunctions(CommitStore);

CommitStore.UpdateInterval = 45000; // 45 seconds
CommitStore.NO_MORE_REVISIONS = null;
CommitStore.UUID_MULTIPLIER = 100;
CommitStore.MAX_BLOCK = 20

CommitStore.Event = {
    CommitsUpdated: "commits-updated",
};

CommitStore.prototype = {
    constructor: CommitStore,
    __proto__: BaseObject.prototype,

    update: function() {
        let self = this
        Object.entries(self.commitsByBranch).forEach(function(branch){
            self.fetch(branch[0], CommitStore.MAX_BLOCK);
        });
    },
    repr: function(commit) {
        if (this.useIdentifiers && commit.identifier)
            return commit.identifier;
        if (commit.hash)
            return commit.hash;
        return null;
    },
    startPeriodicUpdates: function() {
        this.update()
        this.updateTimer = setInterval(this.update.bind(this), CommitStore.UpdateInterval);
    },
    uuidFor: function(data) {
        return data.timestamp * CommitStore.UUID_MULTIPLIER + data.order;
    },
    addCommit: function(commit) {
        if (this.useIdentifiers)
            this.commitsByRef[commit.identifier] = commit;
        this.commitsByRef[commit.hash] = commit;

        if (!this.commitsByBranch[commit.branch])
            this.commitsByBranch[commit.branch] = [];

        let index = 0;
        let didInsert = false;
        while (true) {
            let currentValue = this.commitsByBranch[commit.branch][index];
            if (currentValue && this.uuidFor(commit) == this.uuidFor(currentValue))
                didInsert = true;
            else if (!didInsert && (!currentValue || this.uuidFor(commit) > this.uuidFor(currentValue))) {
                this.commitsByBranch[commit.branch] = [
                    ...this.commitsByBranch[commit.branch].slice(0, index),
                    commit,
                    ...this.commitsByBranch[commit.branch].slice(index)
                ];
                currentValue = commit;
                didInsert = true;
            } else if (!currentValue) {
                if (didInsert)
                    this.dispatchEventToListeners(CommitStore.Event.CommitsUpdated, null);
                return;
            }

            if (this.useIdentifiers)
                this.indexForCommit[currentValue.identifier] = index;
            this.indexForCommit[currentValue.hash] = index;
            index += 1;
        }
    },
    branchPosition: function(ref) {
        data = this.commitsByRef[ref];

        if (!this.useIdentifiers) {
            // Git repositories without identifiers can't give an absolute branch position
            // instead, compute branch position relative to the set of commits we're aware of
            const result = this.indexForCommit[ref];
            if (result == null || !data)
                return null;
            return this.commitsByBranch[data.branch].length - result;
        }

        let identifierParts = ref.split('@');
        if (data)
            identifierParts = data.identifier.split('@');
        
        if (identifierParts.length <= 1)
            return null;
        const identifierCount = identifierParts[0].split('.');
        if (identifierCount.length == 1)
            return Number(identifierCount[0]);
        else if (identifierCount.length == 2)
            return Number(identifierCount[1]);
        return null
    },
    nextRevision: function(branchName, revision) {
        const index = this.indexOfRef(revision);
        if (index < 0)
            return CommitStore.NO_MORE_REVISIONS;
        const commitsOnBranch = this.commitsByBranch[branchName];
        if (commitsOnBranch && index + 1 < commitsOnBranch.length)
            return commitsOnBranch[index + 1].identifier;
        return CommitStore.NO_MORE_REVISIONS;
    },
    indexOfRef: function(ref) {
        const result = this.indexForCommit[ref];
        return result == null ? -1 : result;
    },
};
