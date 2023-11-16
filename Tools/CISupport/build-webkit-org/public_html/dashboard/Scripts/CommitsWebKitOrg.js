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

CommitsWebKitOrg = function(url)
{
    CommitStore.call(this);

    console.assert(url);
    this.url = url;
    this.useIdentifiers = true;
};

BaseObject.addConstructorFunctions(CommitsWebKitOrg);

CommitsWebKitOrg.prototype = {
    constructor: CommitsWebKitOrg,
    __proto__: CommitStore.prototype,

    urlFor: function(ref) {
        return `${this.url}/${ref}`;
    },
    fetch: function(base, count) {
        let self = this;

        JSON.load(`${self.url}/${base}/json`, function(data) {
            self.addCommit(data);

            count -= 1;

            let newIdentifier = data.identifier;
            while (count > 0) {
                const identifierParts = newIdentifier.split('@');
                if (identifierParts.length <= 1)
                    return;
                const identifierCount = identifierParts[0].split('.');

                if (identifierCount.length == 1)
                    newIdentifier = `${Number(identifierCount[0]) - 1}@${data.branch}`;
                else if (identifierCount.length == 2) {
                    const idTail = Number(identifierCount[1]) - 1;
                    if (idTail <= 0)
                        return;
                    newIdentifier = `${identifierCount[0]}.${idTail}@${data.branch}`;
                } else
                    return;

                count -= 1;
                if (!self.commitsByRef[newIdentifier])
                    self.fetch(newIdentifier, count);
            }
        });
    },
};
