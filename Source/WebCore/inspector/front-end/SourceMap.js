/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/**
 * Implements Source Map V3 model. See http://code.google.com/p/closure-compiler/wiki/SourceMaps
 * for format description.
 * @constructor
 * @param {string} sourceMappingURL
 * @param {SourceMapV3} payload
 */
WebInspector.SourceMap = function(sourceMappingURL, payload)
{
    if (!WebInspector.SourceMap.prototype._base64Map) {
        const base64Digits = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
        WebInspector.SourceMap.prototype._base64Map = {};
        for (var i = 0; i < base64Digits.length; ++i)
            WebInspector.SourceMap.prototype._base64Map[base64Digits.charAt(i)] = i;
    }

    this._sourceMappingURL = sourceMappingURL;
    this._mappings = [];
    this._sources = {};
    this._sourceContentByURL = {};
    this._parseMappingPayload(payload);
}

WebInspector.SourceMap.prototype = {
    /**
     * @return {Array.<string>}
     */
    sources: function()
    {
        return Object.keys(this._sources);
    },

    /**
     * @param {string} sourceURL
     * @return {string|undefined}
     */
    sourceContent: function(sourceURL)
    {
        return this._sourceContentByURL[sourceURL];
    },

    /**
     * @param {SourceMapV3} mappingPayload
     */
    _parseMappingPayload: function(mappingPayload)
    {
        if (mappingPayload.sections)
            this._parseSections(mappingPayload.sections);
        else
            this._parseMap(mappingPayload, 0, 0);
    },

    /**
     * @param {Array.<SourceMapV3.Section>} sections
     */
    _parseSections: function(sections)
    {
        for (var i = 0; i < sections.length; ++i) {
            var section = sections[i];
            this._parseMap(section.map, section.offset.line, section.offset.column);
        }
    },

    /**
     * @param {SourceMapV3} map
     * @param {number} lineNumber
     * @param {number} columnNumber
     */
    _parseMap: function(map, lineNumber, columnNumber)
    {
        var sourceIndex = 0;
        var sourceLineNumber = 0;
        var sourceColumnNumber = 0;
        var nameIndex = 0;

        var sources = [];
        var originalToCanonicalURLMap = {};
        for (var i = 0; i < map.sources.length; ++i) {
            var originalSourceURL = map.sources[i];
            var url = this._canonicalizeURL((map.sourceRoot ? map.sourceRoot + "/" : "") + originalSourceURL, this._sourceMappingURL);
            originalToCanonicalURLMap[originalSourceURL] = url;
            sources.push(url);
            this._sources[url] = true;

            if (map.sourcesContent && map.sourcesContent[i])
                this._sourceContentByURL[url] = map.sourcesContent[i];
        }

        var stringCharIterator = new WebInspector.SourceMap.StringCharIterator(map.mappings);
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
    },

    /**
     * @param {string} char
     * @return {boolean}
     */
    _isSeparator: function(char)
    {
        return char === "," || char === ";";
    },

    /**
     * @param {WebInspector.SourceMap.StringCharIterator} stringCharIterator
     * @return {number}
     */
    _decodeVLQ: function(stringCharIterator)
    {
        // Read unsigned value.
        var result = 0;
        var shift = 0;
        do {
            var digit = this._base64Map[stringCharIterator.next()];
            result += (digit & this._VLQ_BASE_MASK) << shift;
            shift += this._VLQ_BASE_SHIFT;
        } while (digit & this._VLQ_CONTINUATION_MASK);

        // Fix the sign.
        var negative = result & 1;
        result >>= 1;
        return negative ? -result : result;
    },

    /**
     * @param {string} url
     * @param {string} baseURL
     */
    _canonicalizeURL: function(url, baseURL)
    {
        if (!url || !baseURL || url.asParsedURL() || url.substring(0, 5) === "data:")
            return url;

        var base = baseURL.asParsedURL();
        if (!base)
            return url;

        var baseHost = base.scheme + "://" + base.host + (base.port ? ":" + base.port : "");
        if (url[0] === "/")
            return baseHost + url;
        return baseHost + base.folderPathComponents + "/" + url;
    },

    _VLQ_BASE_SHIFT: 5,
    _VLQ_BASE_MASK: (1 << 5) - 1,
    _VLQ_CONTINUATION_MASK: 1 << 5
}

/**
 * @constructor
 * @param {string} string
 */
WebInspector.SourceMap.StringCharIterator = function(string)
{
    this._string = string;
    this._position = 0;
}

WebInspector.SourceMap.StringCharIterator.prototype = {
    /**
     * @return {string}
     */
    next: function()
    {
        return this._string.charAt(this._position++);
    },

    /**
     * @return {string}
     */
    peek: function()
    {
        return this._string.charAt(this._position);
    },

    /**
     * @return {boolean}
     */
    hasNext: function()
    {
        return this._position < this._string.length;
    }
}

/**
 * @constructor
 * @extends WebInspector.SourceMap
 * @param {string} sourceMappingURL
 * @param {SourceMapV3} payload
 */
WebInspector.PositionBasedSourceMap = function(sourceMappingURL, payload)
{
    this._reverseMappingsBySourceURL = {};
    WebInspector.SourceMap.call(this, sourceMappingURL, payload);
}

WebInspector.PositionBasedSourceMap.prototype = {
    /**
     * @param {number} lineNumber in compiled resource
     * @param {number} columnNumber in compiled resource
     */
    findEntry: function(lineNumber, columnNumber)
    {
        var first = 0;
        var count = this._mappings.length;
        while (count > 1) {
          var step = count >> 1;
          var middle = first + step;
          var mapping = this._mappings[middle];
          if (lineNumber < mapping[0] || (lineNumber == mapping[0] && columnNumber < mapping[1]))
              count = step;
          else {
              first = middle;
              count -= step;
          }
        }
        return this._mappings[first];
    },

    /**
     * @param {string} sourceURL of the originating resource
     * @param {number} lineNumber in the originating resource
     * @return {Array}
     */
    findEntryReversed: function(sourceURL, lineNumber)
    {
        var mappings = this._reverseMappingsBySourceURL[sourceURL];
        for ( ; lineNumber < mappings.length; ++lineNumber) {
            var mapping = mappings[lineNumber];
            if (mapping)
                return mapping;
        }
        return this._mappings[0];
    },

    /**
     * @override
     */
    _parseMap: function(map, lineNumber, columnNumber)
    {
        WebInspector.SourceMap.prototype._parseMap.call(this, map, lineNumber, columnNumber);

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
    },

    __proto__: WebInspector.SourceMap.prototype
}

/**
 * @constructor
 * @extends WebInspector.SourceMap
 * @param {string} sourceMappingURL
 * @param {SourceMapV3} payload
 */
WebInspector.RangeBasedSourceMap = function(sourceMappingURL, payload)
{
    WebInspector.SourceMap.call(this, sourceMappingURL, payload);

    // Empty mappings should not normally be found in a range-based map.
    function callback(value)
    {
        return !!value[2];
    }
    this._mappings = this._mappings.filter(callback);
}

WebInspector.RangeBasedSourceMap.MappingComparator = function(a, b)
{
    if (a[0] !== b[0])
        return a[0] - b[0];
    return a[1] - b[1];
}

WebInspector.RangeBasedSourceMap.prototype = {
    /**
     * @param {number} lineNumber in the compiled resource
     * @param {number} columnNumber in the compiled resource
     * @return {?WebInspector.RangeBasedSourceMap.SourceRange}
     */
    findSourceRange: function(lineNumber, columnNumber)
    {
        var comparator = WebInspector.RangeBasedSourceMap.MappingComparator;
        var lookupEntry = [lineNumber, columnNumber];
        var index = binarySearch(lookupEntry, this._mappings, comparator);
        if (index >= 0) {
            if (index % 2) {
                // Range end hit. Check if there's a following range that starts from the same position.
                if (index + 1 >= this._mappings.length || comparator(lookupEntry, this._mappings[index + 1]))
                    return null;
                return this._rangeForStartIndex(index + 1);
            }

            return this._rangeForStartIndex(index);
        }

        index = -(index + 1);
        if ((index % 2) && comparator(lookupEntry, this._mappings[index - 1]) >= 0 && comparator(lookupEntry, this._mappings[index]) <= 0)
            return this._rangeForStartIndex(index - 1);

        return null;
    },

    /**
     * @param {number} index
     * @return {?WebInspector.RangeBasedSourceMap.SourceRange}
     */
    _rangeForStartIndex: function(index)
    {
        var startEntry = this._mappings[index];
        var endEntry = this._mappings[index + 1];
        if (startEntry[2] !== endEntry[2]) {
            console.error("Mismatched source URLs in adjacent range-based sourcemap entries: %s vs %s", JSON.stringify(startEntry), JSON.stringify(endEntry));
            return null;
        }

        return new WebInspector.RangeBasedSourceMap.SourceRange(startEntry[2], startEntry[3], startEntry[4], endEntry[3], endEntry[4]);
    },

    __proto__: WebInspector.SourceMap.prototype
}

/**
 * @constructor
 * @param {string} url
 * @param {number} startLine
 * @param {number} startColumn
 * @param {number} endLine
 * @param {number} endColumn
 */
WebInspector.RangeBasedSourceMap.SourceRange = function(url, startLine, startColumn, endLine, endColumn)
{
    this.url = url;
    this.startLine = startLine;
    this.endLine = endLine;
    this.startColumn = startColumn;
    this.endColumn = endColumn;
}
