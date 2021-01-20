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

WI.ResourceQueryController = class ResourceQueryController extends WI.Object
{
    constructor()
    {
        super();

        this._resourceDataMap = new Map;
    }

    // Public

    addResource(resource)
    {
        this._resourceDataMap.set(resource, {});
    }

    removeResource(resource)
    {
        this._resourceDataMap.delete(resource);
    }

    reset()
    {
        this._resourceDataMap.clear();
    }

    executeQuery(query)
    {
        if (!query || !this._resourceDataMap.size)
            return [];

        query = query.removeWhitespace().toLowerCase();

        let cookie = null;
        if (query.includes(":")) {
            let [newQuery, lineNumber, columnNumber] = query.split(":");
            query = newQuery;
            lineNumber = lineNumber ? parseInt(lineNumber, 10) - 1 : 0;
            columnNumber = columnNumber ? parseInt(columnNumber, 10) - 1 : 0;
            cookie = {lineNumber, columnNumber};
        }

        let results = [];
        for (let [resource, cachedData] of this._resourceDataMap) {
            if (!cachedData.searchString) {
                let displayName = resource.displayName;
                cachedData.searchString = displayName.toLowerCase();
                cachedData.specialCharacterIndices = this._findSpecialCharacterIndices(displayName);
            }

            let matches = this._findQueryMatches(query, cachedData.searchString, cachedData.specialCharacterIndices);
            if (matches.length)
                results.push(new WI.ResourceQueryResult(resource, matches, cookie));
        }

        // Resources are sorted in descending order by rank. Resources of equal
        // rank are sorted by display name.
        return results.sort((a, b) => {
            if (a.rank === b.rank)
                return a.resource.displayName.extendedLocaleCompare(b.resource.displayName);
            return b.rank - a.rank;
        });
    }

    // Private

    _findQueryMatches(query, searchString, specialCharacterIndices)
    {
        if (query.length > searchString.length)
            return [];

        let matches = [];
        let queryIndex = 0;
        let searchIndex = 0;
        let specialIndex = 0;
        let deadBranches = new Array(query.length).fill(Infinity);
        let type = WI.ResourceQueryMatch.Type.Special;

        function pushMatch(index)
        {
            matches.push(new WI.ResourceQueryMatch(type, index, queryIndex));
            searchIndex = index + 1;
            queryIndex++;
        }

        function matchNextSpecialCharacter()
        {
            if (specialIndex >= specialCharacterIndices.length)
                return false;

            let originalSpecialIndex = specialIndex;
            while (specialIndex < specialCharacterIndices.length) {
                // Normal character matching can move past special characters,
                // so advance the special character index if it's before the
                // current search string position.
                let index = specialCharacterIndices[specialIndex++];
                if (index < searchIndex)
                    continue;

                if (query[queryIndex] === searchString[index]) {
                    pushMatch(index);
                    return true;
                }
            }

            specialIndex = originalSpecialIndex;
            return false;
        }

        function backtrack()
        {
            while (matches.length) {
                queryIndex--;

                let lastMatch = matches.pop();
                if (lastMatch.type !== WI.ResourceQueryMatch.Type.Special)
                    continue;

                deadBranches[lastMatch.queryIndex] = lastMatch.index;
                searchIndex = matches.lastValue ? matches.lastValue.index + 1 : 0;
                return true;
            }

            return false;
        }

        while (queryIndex < query.length && searchIndex <= searchString.length) {
            if (type === WI.ResourceQueryMatch.Type.Special && !matchNextSpecialCharacter())
                type = WI.ResourceQueryMatch.Type.Normal;

            if (type === WI.ResourceQueryMatch.Type.Normal) {
                let index = searchString.indexOf(query[queryIndex], searchIndex);
                if (index >= 0 && index < deadBranches[queryIndex]) {
                    pushMatch(index);
                    type = WI.ResourceQueryMatch.Type.Special;
                } else if (!backtrack())
                    return [];
            }
        }

        if (queryIndex < query.length)
            return [];

        return matches;
    }

    _findSpecialCharacterIndices(string)
    {
        if (!string.length)
            return [];

        const filenameSeparators = "_.-";

        // Special characters include the following:
        // 1. The first character.
        // 2. Uppercase characters that follow a lowercase letter.
        // 3. Filename separators and the first character following the separator.
        let indices = [0];

        for (let i = 1; i < string.length; ++i) {
            let character = string[i];
            let isSpecial = false;

            if (filenameSeparators.includes(character))
                isSpecial = true;
            else {
                let previousCharacter = string[i - 1];
                let previousCharacterIsSeparator = filenameSeparators.includes(previousCharacter);
                if (previousCharacterIsSeparator)
                    isSpecial = true;
                else if (character.isUpperCase() && previousCharacter.isLowerCase())
                    isSpecial = true;
            }

            if (isSpecial)
                indices.push(i);
        }

        return indices;
    }
};
