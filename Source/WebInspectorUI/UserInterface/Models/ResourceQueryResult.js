/*
 * Copyright (C) 2016 Apple Inc. All rights reserved.
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

WebInspector.ResourceQueryResult = class QueryResult extends WebInspector.Object
{
    constructor(resource, matches)
    {
        console.assert(matches.length, "Query matches list can't be empty.");

        super();

        this._resource = resource;
        this._matches = matches;
    }

    // Public

    get resource() { return this._resource; }

    get rank()
    {
        if (this._rank === undefined)
            this._calculateRank();

        return this._rank;
    }

    get matchingTextRanges()
    {
        if (!this._matchingTextRanges)
            this._matchingTextRanges = this._createMatchingTextRanges();

        return this._matchingTextRanges;
    }

    // Private

    _calculateRank()
    {
        const specialWeight = 50;
        const normalWeight = 10;
        const consecutiveMatchWeight = 5;

        this._rank = 0;

        let previousMatch = null;
        for (let match of this._matches) {
            this._rank += match.type === WebInspector.ResourceQueryMatch.Type.Special ? specialWeight : normalWeight;
            if (previousMatch && previousMatch.index === match.index - 1)
                this._rank += consecutiveMatchWeight;

            previousMatch = match;

            // The match index is deducted from the total rank, so matches that
            // occur closer to the beginning of the string are ranked higher.
            this._rank -= match.index;
        }
    }

    _createMatchingTextRanges()
    {
        if (!this._matches.length)
            return [];

        let ranges = [];
        let startIndex = this._matches[0].index;
        let endIndex = startIndex;
        for (let i = 1; i < this._matches.length; ++i) {
            let match = this._matches[i];

            // Increment endIndex for consecutive match.
            if (match.index === endIndex + 1) {
                endIndex++;
                continue;
            }

            // Begin a new range when a gap between this match and the previous match is found.
            ranges.push(new WebInspector.TextRange(0, startIndex, 0, endIndex + 1));
            startIndex = match.index;
            endIndex = startIndex;
        }

        ranges.push(new WebInspector.TextRange(0, startIndex, 0, endIndex + 1));
        return ranges;
    }

    // Testing

    __test_createMatchesMask()
    {
        let filename = this._resource.displayName;
        let lastIndex = -1;
        let result = "";

        for (let match of this._matches) {
            let gap = " ".repeat(match.index - lastIndex - 1);
            result += gap;
            result += filename[match.index];
            lastIndex = match.index;
        }

        return result;
    }
};
