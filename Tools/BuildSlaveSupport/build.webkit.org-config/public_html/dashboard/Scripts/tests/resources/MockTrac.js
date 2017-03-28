/*
 * Copyright (C) 2016 Apple, Inc. All rights reserved.
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

MockTrac = function(projectIdentifier)
{
    if (projectIdentifier) {
        var options = { };
        options[Trac.ProjectIdentifier] = projectIdentifier;
        Trac.call(this, "https://trac.webkit.org/", options);
    } else {
        Trac.call(this, "https://trac.webkit.org/");
    }
};

BaseObject.addConstructorFunctions(MockTrac);

MockTrac.prototype = {
    constructor: MockTrac,
    __proto__: Trac.prototype,

    get oldestRecordedRevisionNumber()
    {
        return "33018";
    },

    get latestRecordedRevisionNumber()
    {
        return "33022";
    },

    loadMoreHistoricalData: function()
    {
    },
};

MockTrac.EXAMPLE_TRAC_COMMITS = [
    {
        "revisionNumber": "33018",
        "link": "https://trac.webkit.org/changeset/33018",
        "title": { innerHTML: "commit message" },
        "author": "john@webkit.org",
        "date": new Date("2015-11-15T17:05:44.000Z"),
        "description": "description",
        "containsBranchLocation": true,
        "branches": ["trunk"]
    },
    {
        "revisionNumber": "33019",
        "link": "https://trac.webkit.org/changeset/33019",
        "title": { innerHTML: "commit message" },
        "author": "paul@webkit.org",
        "date": new Date("2015-11-16T01:18:23.000Z"),
        "description": "description",
        "containsBranchLocation": true,
        "branches": ["someOtherBranch"]
    },
    {
        "revisionNumber": "33020",
        "link": "https://trac.webkit.org/changeset/33020",
        "title": { innerHTML: "commit message" },
        "author": "george@webkit.org",
        "date": new Date("2015-11-16T01:19:27.000Z"),
        "description": "description",
        "containsBranchLocation": true,
        "branches": ["trunk"]
    },
    {
        "revisionNumber": "33021",
        "link": "https://trac.webkit.org/changeset/33021",
        "title": { innerHTML: "commit message" },
        "author": "ringo@webkit.org",
        "date": new Date("2015-11-16T01:20:58.000Z"),
        "description": "description",
        "containsBranchLocation": true,
        "branches": ["someOtherBranch"]
    },
    {
        "revisionNumber": "33022",
        "link": "https://trac.webkit.org/changeset/33022",
        "title": { innerHTML: "commit message" },
        "author": "bob@webkit.org",
        "date": new Date("2015-11-16T01:22:01.000Z"),
        "description": "description",
        "containsBranchLocation": true,
        "branches": ["trunk"]
    }
];

MockTrac.recordedCommitIndicesByRevisionNumber = {
    33018: 0,
    33019: 1,
    33020: 2,
    33021: 3,
    33022: 4,
    33023: 5,
    33024: 6,
    33025: 7
};
