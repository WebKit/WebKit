// Copyright (C) 2020 Apple Inc. All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions
// are met:
// 1. Redistributions of source code must retain the above copyright
//    notice, this list of conditions and the following disclaimer.
// 2. Redistributions in binary form must reproduce the above copyright
//    notice, this list of conditions and the following disclaimer in the
//    documentation and/or other materials provided with the distribution.
//
// THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
// THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
// PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
// BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
// CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
// SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
// INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
// CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
// ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
// THE POSSIBILITY OF SUCH DAMAGE.

class _ArchiveRouter {
    constructor() {
        this.routes = {};
        fetch('archive-routes').then(response => {
            let self = this;
            response.json().then(json => {
                if (Array.isArray(json)) {
                    console.error(JSON.stringify(json, null, 4));
                    return;
                }
                this.routes = json;
            });
        }).catch(error => {
            console.error(JSON.stringify(error, null, 4));
        });
    }
    _determineArgumentFromAncestry(argument, defaultValue, suite=null, mode=null) {
        let result = defaultValue;
        let routes = this.routes;

        if (argument in routes)
            result = routes[argument];

        [suite, mode].forEach(ancestry => {
            if (!ancestry)
                return;
            if (!(ancestry in routes))
                return;
            routes = routes[ancestry];
            if (argument in routes)
                result = routes[argument];
        });
        return result;
    }
    hasArchive(suite=null, mode=null) {
        return this._determineArgumentFromAncestry('enabled', false, suite, mode);
    }
    pathFor(suite=null, mode=null, test=null) {
        if (!this.hasArchive(suite))
            return null;

        let candidate = this._determineArgumentFromAncestry('path', '', suite, mode);
        if (!test || !candidate.startsWith('*'))
            return candidate;
        return test.substring(0, test.lastIndexOf('.')) + candidate.substring(1);
    }
    labelFor(suite=null, mode=null) {
        if (!this.hasArchive(suite))
            return null;

        return this._determineArgumentFromAncestry('label', 'Result archive', suite, mode);
    }
};

const ArchiveRouter = new _ArchiveRouter();

export {ArchiveRouter};
