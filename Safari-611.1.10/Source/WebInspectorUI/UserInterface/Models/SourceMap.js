/*
 * Copyright (C) 2013 Apple Inc. All rights reserved.
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

WI.SourceMap = class SourceMap
{
    constructor(sourceMappingURL, payload, originalSourceCode)
    {
        if (!WI.SourceMap._base64Map) {
            var base64Digits = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
            WI.SourceMap._base64Map = {};
            for (var i = 0; i < base64Digits.length; ++i)
                WI.SourceMap._base64Map[base64Digits.charAt(i)] = i;
        }

        this._originalSourceCode = originalSourceCode || null;
        this._sourceMapResources = {};
        this._sourceMapResourcesList = [];

        this._sourceMappingURL = sourceMappingURL;
        this._reverseMappingsBySourceURL = {};
        this._mappings = [];
        this._sources = {};
        this._sourceRoot = null;
        this._sourceContentByURL = {};
        this._parseMappingPayload(payload);
    }

    // Public

    get originalSourceCode()
    {
        return this._originalSourceCode;
    }

    get sourceMappingBasePathURLComponents()
    {
        if (this._sourceMappingURLBasePathComponents)
            return this._sourceMappingURLBasePathComponents;

        if (this._sourceRoot) {
            var baseURLPath = absoluteURL(this._sourceRoot, this._sourceMappingURL);
            console.assert(baseURLPath);
            if (baseURLPath) {
                var urlComponents = parseURL(baseURLPath);
                if (!/\/$/.test(urlComponents.path))
                    urlComponents.path += "/";
                this._sourceMappingURLBasePathComponents = urlComponents;
                return this._sourceMappingURLBasePathComponents;
            }
        }

        var urlComponents = parseURL(this._sourceMappingURL);

        // Fallback for JavaScript debuggable named scripts that may not have a complete URL.
        if (!urlComponents.path)
            urlComponents.path = this._sourceMappingURL || "";

        urlComponents.path = urlComponents.path.substr(0, urlComponents.path.lastIndexOf(urlComponents.lastPathComponent));
        urlComponents.lastPathComponent = null;
        this._sourceMappingURLBasePathComponents = urlComponents;
        return this._sourceMappingURLBasePathComponents;
    }

    get resources()
    {
        return this._sourceMapResourcesList;
    }

    addResource(resource)
    {
        console.assert(!(resource.url in this._sourceMapResources));
        this._sourceMapResources[resource.url] = resource;
        this._sourceMapResourcesList.push(resource);
    }

    resourceForURL(url)
    {
        return this._sourceMapResources[url];
    }

    sources()
    {
        return Object.keys(this._sources);
    }

    sourceContent(sourceURL)
    {
        return this._sourceContentByURL[sourceURL];
    }

    _parseMappingPayload(mappingPayload)
    {
        if (mappingPayload.sections)
            this._parseSections(mappingPayload.sections);
        else
            this._parseMap(mappingPayload, 0, 0);
    }

    _parseSections(sections)
    {
        for (var i = 0; i < sections.length; ++i) {
            var section = sections[i];
            this._parseMap(section.map, section.offset.line, section.offset.column);
        }
    }

    findEntry(lineNumber, columnNumber)
    {
        var first = 0;
        var count = this._mappings.length;
        while (count > 1) {
            var step = count >> 1;
            var middle = first + step;
            var mapping = this._mappings[middle];
            if (lineNumber < mapping[0] || (lineNumber === mapping[0] && columnNumber < mapping[1]))
                count = step;
            else {
                first = middle;
                count -= step;
            }
        }
        var entry = this._mappings[first];
        if (!first && entry && (lineNumber < entry[0] || (lineNumber === entry[0] && columnNumber < entry[1])))
            return null;
        return entry;
    }

    findEntryReversed(sourceURL, lineNumber)
    {
        var mappings = this._reverseMappingsBySourceURL[sourceURL];
        for ( ; lineNumber < mappings.length; ++lineNumber) {
            var mapping = mappings[lineNumber];
            if (mapping)
                return mapping;
        }
        return this._mappings[0];
    }

    _parseMap(map, lineNumber, columnNumber)
    {
        var sourceIndex = 0;
        var sourceLineNumber = 0;
        var sourceColumnNumber = 0;
        var nameIndex = 0;

        var sources = [];
        var originalToCanonicalURLMap = {};
        for (var i = 0; i < map.sources.length; ++i) {
            var originalSourceURL = map.sources[i];
            var href = originalSourceURL;
            if (map.sourceRoot && href.charAt(0) !== "/")
                href = map.sourceRoot.replace(/\/+$/, "") + "/" + href;
            var url = absoluteURL(href, this._sourceMappingURL) || href;
            originalToCanonicalURLMap[originalSourceURL] = url;
            sources.push(url);
            this._sources[url] = true;

            if (map.sourcesContent && map.sourcesContent[i])
                this._sourceContentByURL[url] = map.sourcesContent[i];
        }

        this._sourceRoot = map.sourceRoot || null;

        var stringCharIterator = new WI.SourceMap.StringCharIterator(map.mappings);
        var sourceURL = sources[sourceIndex];

        while (true) {
            if (stringCharIterator.peek() === ",")
                stringCharIterator.next();
            else {
                while (stringCharIterator.peek() === ";") {
                    lineNumber += 1;
                    columnNumber = 0;
                    stringCharIterator.next();
                }
                if (!stringCharIterator.hasNext())
                    break;
            }

            columnNumber += this._decodeVLQ(stringCharIterator);
            if (this._isSeparator(stringCharIterator.peek())) {
                this._mappings.push([lineNumber, columnNumber]);
                continue;
            }

            var sourceIndexDelta = this._decodeVLQ(stringCharIterator);
            if (sourceIndexDelta) {
                sourceIndex += sourceIndexDelta;
                sourceURL = sources[sourceIndex];
            }
            sourceLineNumber += this._decodeVLQ(stringCharIterator);
            sourceColumnNumber += this._decodeVLQ(stringCharIterator);
            if (!this._isSeparator(stringCharIterator.peek()))
                nameIndex += this._decodeVLQ(stringCharIterator);

            this._mappings.push([lineNumber, columnNumber, sourceURL, sourceLineNumber, sourceColumnNumber]);
        }

        for (var i = 0; i < this._mappings.length; ++i) {
            var mapping = this._mappings[i];
            var url = mapping[2];
            if (!url)
                continue;
            if (!this._reverseMappingsBySourceURL[url])
                this._reverseMappingsBySourceURL[url] = [];
            var reverseMappings = this._reverseMappingsBySourceURL[url];
            var sourceLine = mapping[3];
            if (!reverseMappings[sourceLine])
                reverseMappings[sourceLine] = [mapping[0], mapping[1]];
        }
    }

    _isSeparator(char)
    {
        return char === "," || char === ";";
    }

    _decodeVLQ(stringCharIterator)
    {
        // Read unsigned value.
        var result = 0;
        var shift = 0;
        do {
            var digit = WI.SourceMap._base64Map[stringCharIterator.next()];
            result += (digit & WI.SourceMap.VLQ_BASE_MASK) << shift;
            shift += WI.SourceMap.VLQ_BASE_SHIFT;
        } while (digit & WI.SourceMap.VLQ_CONTINUATION_MASK);

        // Fix the sign.
        var negative = result & 1;
        result >>= 1;
        return negative ? -result : result;
    }
};

WI.SourceMap.VLQ_BASE_SHIFT = 5;
WI.SourceMap.VLQ_BASE_MASK = (1 << 5) - 1;
WI.SourceMap.VLQ_CONTINUATION_MASK = 1 << 5;

WI.SourceMap.StringCharIterator = class StringCharIterator
{
    constructor(string)
    {
        this._string = string;
        this._position = 0;
    }

    next()
    {
        return this._string.charAt(this._position++);
    }

    peek()
    {
        return this._string.charAt(this._position);
    }

    hasNext()
    {
        return this._position < this._string.length;
    }
};
