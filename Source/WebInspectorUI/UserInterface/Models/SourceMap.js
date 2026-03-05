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
    constructor(originalSourceCode, sourceMappingURL, sourceRoot, dataForSourceMapResourceURL, mappings)
    {
        this._originalSourceCode = originalSourceCode;
        this._sourceMappingURL = sourceMappingURL;
        this._sourceRoot = sourceRoot;
        this._mappings = mappings;

        this._generatedPositionForOriginalLineForURL = new Map;
        for (let [generatedLine, generatedColumn, sourceURL, originalLine, originalColumn] of this._mappings) {
            if (!sourceURL)
                continue;
            let generatedPositionForOriginalLine = this._generatedPositionForOriginalLineForURL.getOrInsert(sourceURL, new Map);
            generatedPositionForOriginalLine.getOrInsert(originalLine, [generatedLine, generatedColumn]);
        }

        this._sourceMapResourceForURL = new Map;
        for (let [sourceURL, data] of dataForSourceMapResourceURL)
            this._sourceMapResourceForURL.set(sourceURL, new WI.SourceMapResource(this, sourceURL, data));

        WI.SourceMap._instances.add(this);
    }

    // Static

    static get instances()
    {
        return Array.from(WI.SourceMap._instances);
    }

    static async fromJSON(originalSourceCode, sourceMappingURL, payload)
    {
        if (!payload || typeof payload !== "object")
            throw WI.SourceMap._invalidPropertyError(WI.unlocalizedString("json"));

        if (payload.version !== 3)
            throw WI.SourceMap._invalidPropertyError(WI.unlocalizedString("version"));

        if ("file" in payload && typeof payload.file !== "string")
            throw WI.SourceMap._invalidPropertyError(WI.unlocalizedString("file"));
        // Currently `file` is unused, but check it anyways for conformance.

        // https://tc39.es/ecma426/#sec-DecodeIndexSourceMap

        if (!Array.isArray(payload.sections))
            payload = {sections: [{offset: {line: 0, column: 0}, map: payload}]};

        if ("mappings" in payload)
            throw WI.SourceMap._invalidPropertyError(WI.unlocalizedString("mappings"));

        let sourceRoot = null;
        let dataForSourceMapResourceURL = new Map;
        let mappings = [];

        let lastLineNumber = -1;
        let lastColumnNumber = -1;
        for (let {offset, map} of payload.sections) {
            if (!offset || typeof offset !== "object")
                throw WI.SourceMap._invalidPropertyError(WI.unlocalizedString("offset"));

            if (!map || typeof map !== "object")
                throw WI.SourceMap._invalidPropertyError(WI.unlocalizedString("map"));

            let offsetLineNumber = offset.line;
            if (!Number.isInteger(offsetLineNumber) || offsetLineNumber < 0 || offsetLineNumber < lastLineNumber)
                throw WI.SourceMap._invalidPropertyError(WI.unlocalizedString("offset.line"));

            let offsetColumnNumber = offset.column;
            if (!Number.isInteger(offsetColumnNumber) || offsetColumnNumber < 0 || (offsetLineNumber === lastLineNumber && offsetColumnNumber <= lastColumnNumber))
                throw WI.SourceMap._invalidPropertyError(WI.unlocalizedString("offset.column"));

            // https://tc39.es/ecma426/#sec-DecodeSourceMap

            if (typeof map.mappings !== "string")
                throw WI.SourceMap._invalidPropertyError(WI.unlocalizedString("mappings"));

            if (!Array.isArray(map.sources))
                throw WI.SourceMap._invalidPropertyError(WI.unlocalizedString("sources"));

            if ("sourceRoot" in map && typeof map.sourceRoot !== "string")
                throw WI.SourceMap._invalidPropertyError(WI.unlocalizedString("sourceRoot"));
            sourceRoot = map.sourceRoot || null;

            if ("sourcesContent" in map && !Array.isArray(map.sourcesContent))
                throw WI.SourceMap._invalidPropertyError(WI.unlocalizedString("sourcesContent"));

            if ("ignoreList" in map && (!Array.isArray(map.ignoreList) || map.ignoreList.some((index) => index !== null && (!Number.isInteger(index) || index < 0 || index >= map.sources.length))))
                throw WI.SourceMap._invalidPropertyError(WI.unlocalizedString("ignoreList"));
            let ignoreList = new Set(map.ignoreList || []);

            if ("names" in map && (!Array.isArray(map.names) || map.names.some((name) => typeof name !== "string")))
                throw WI.SourceMap._invalidPropertyError(WI.unlocalizedString("names"));
            let names = map.names || [];
            // Currently `names` is unused, but check it anyways for conformance.

            let sourceURLs = [];
            for (let i = 0; i < map.sources.length; ++i) {
                let sourceURL = map.sources[i];
                if (sourceURL !== null && typeof sourceURL !== "string")
                    throw WI.SourceMap._invalidPropertyError(WI.unlocalizedString("sources"));

                if (sourceURL) {
                    if (sourceRoot && sourceURL.charAt(0) !== "/")
                        sourceURL = sourceRoot.replace(/\/+$/, "") + "/" + sourceURL;
                    sourceURL = absoluteURL(sourceURL, sourceMappingURL) || sourceURL;
                }

                sourceURLs.push(sourceURL);

                let inlineContent = null;
                if (map.sourcesContent) {
                    inlineContent = map.sourcesContent[i];
                    if (inlineContent !== null && typeof inlineContent !== "string")
                        throw WI.SourceMap._invalidPropertyError(WI.unlocalizedString("sourcesContent"));
                }

                dataForSourceMapResourceURL.set(sourceURL, {
                    inlineContent,
                    ignored: ignoreList.has(i),
                });
            }

            // https://tc39.es/ecma426/#sec-mappings

            let originalLine = 0;
            let originalColumn = 0;
            let sourceURLIndex = 0;
            let nameIndex = 0;
            let generatedLine = offsetLineNumber;
            let generatedColumn = offsetColumnNumber;
            let vlq = new WI.SourceMap._VLQ(WI.SourceMap._invalidPropertyError("mappings"), map.mappings);

            const workInterval = 10;
            let startTime = Date.now();
            while (true) {
                if (vlq.peekNextCharacter() === ",")
                    vlq.takeNextCharacter();
                else {
                    while (vlq.peekNextCharacter() === ";") {
                        ++generatedLine;
                        generatedColumn = 0;
                        vlq.takeNextCharacter();
                    }
                    if (!vlq.hasNextCharacter())
                        break;
                }

                let relativeGeneratedColumn = vlq.decode();
                if (relativeGeneratedColumn === WI.SourceMap._VLQ.AtSeparator)
                    throw WI.SourceMap._invalidPropertyError(WI.unlocalizedString("mappings"));

                generatedColumn += relativeGeneratedColumn;
                if (generatedColumn < 0)
                    throw WI.SourceMap._invalidPropertyError(WI.unlocalizedString("mappings"));

                let relativeSourceURLIndex = vlq.decode();
                let relativeOriginalLine = vlq.decode();
                let relativeOriginalColumn = vlq.decode();
                if (relativeOriginalColumn !== WI.SourceMap._VLQ.AtSeparator) {
                    sourceURLIndex += relativeSourceURLIndex;
                    originalLine += relativeOriginalLine;
                    originalColumn += relativeOriginalColumn;
                    if (sourceURLIndex < 0 || sourceURLIndex >= sourceURLs.length || originalLine < 0 || originalColumn < 0)
                        throw WI.SourceMap._invalidPropertyError(WI.unlocalizedString("mappings"));

                    let relativeNameIndex = vlq.decode();
                    if (relativeNameIndex !== WI.SourceMap._VLQ.AtSeparator) {
                        nameIndex += relativeNameIndex;
                        if (nameIndex < 0 || nameIndex >= names.length)
                            throw WI.SourceMap._invalidPropertyError(WI.unlocalizedString("mappings"));
                        // Currently `names` is unused, but check it anyways for conformance.
                    }

                    if (vlq.hasNextCharacter() && !vlq.atSeparator())
                        throw WI.SourceMap._invalidPropertyError(WI.unlocalizedString("mappings"));

                    mappings.push([generatedLine, generatedColumn, sourceURLs[sourceURLIndex], originalLine, originalColumn]);
                } else {
                    if (relativeSourceURLIndex !== WI.SourceMap._VLQ.AtSeparator)
                        throw WI.SourceMap._invalidPropertyError(WI.unlocalizedString("mappings"));

                    console.assert(relativeOriginalLine === WI.SourceMap._VLQ.AtSeparator, relativeOriginalLine);
                    mappings.push([generatedLine, generatedColumn]);
                }

                if (Date.now() - startTime > workInterval) {
                    await Promise.delay(); // yield

                    startTime = Date.now();
                }
            }

            lastLineNumber = offsetLineNumber;
            lastColumnNumber = offsetColumnNumber;
        }

        // Ensure ordering for binary search in `findOriginalPosition` in case there are negative offsets.
        mappings.sort((a, b) => {
            return a[0] - b[0] // generatedLine
                || a[1] - b[1] // generatedColumn
                || !a[2] - !b[2] // sourceURL (if present)
                || (a[3] ?? 0) - (b[3] ?? 0) // originalLine (if present)
                || (a[4] ?? 0) - (b[4] ?? 0); // originalColumn (if present)
        });

        return new WI.SourceMap(originalSourceCode, sourceMappingURL, sourceRoot, dataForSourceMapResourceURL, mappings);
    }

    static _invalidPropertyError(property)
    {
        const message = WI.UIString("invalid \u0022%s\u0022", "invalid \u0022%s\u0022 @ Source Map", "Error message template when failing to parse a JS source map.");
        return message.format(property);
    }

    // Public

    get originalSourceCode() { return this._originalSourceCode; }
    get sourceMappingURL() { return this._sourceMappingURL; }

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

        urlComponents.path = urlComponents.path.substring(0, urlComponents.path.lastIndexOf(urlComponents.lastPathComponent));
        urlComponents.lastPathComponent = null;
        this._sourceMappingURLBasePathComponents = urlComponents;
        return this._sourceMappingURLBasePathComponents;
    }

    get resources()
    {
        return Array.from(this._sourceMapResourceForURL.values());
    }

    calculateBlackboxSourceRangesForProtocol()
    {
        console.assert(this._originalSourceCode instanceof WI.Script, this._originalSourceCode);

        let blackboxedURLs = {};

        let sourceRanges = [];

        let startLine = undefined;
        let startColumn = undefined;
        for (let [lineNumber, columnNumber, url] of this._mappings) {
            if (!url)
                continue;

            blackboxedURLs[url] ??= !!WI.debuggerManager.blackboxDataForSourceCode(this._sourceMapResourceForURL.get(url));

            if (blackboxedURLs[url]) {
                startLine ??= lineNumber;
                startColumn ??= columnNumber;
            } else if (startLine !== undefined && startColumn !== undefined) {
                sourceRanges.push(startLine, startColumn, lineNumber, columnNumber);

                startLine = undefined;
                startColumn = undefined;
            }
        }
        if (startLine !== undefined && startColumn !== undefined)
            sourceRanges.push(startLine, startColumn, this._originalSourceCode.range.endLine, this._originalSourceCode.range.endColumn);

        return sourceRanges;
    }

    findOriginalPosition(lineNumber, columnNumber)
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
        if (!entry || entry.length !== 5)
            return null;
        if (!first && (lineNumber < entry[0] || (lineNumber === entry[0] && columnNumber < entry[1])))
            return null;
        let sourceMapResource = this._sourceMapResourceForURL.get(entry[2]);
        if (!sourceMapResource)
            return null;
        return [sourceMapResource, entry[3], entry[4]];
    }

    findGeneratedPosition(sourceURL, lineNumber)
    {
        let generatedPositionForOriginalLine = this._generatedPositionForOriginalLineForURL.get(sourceURL);
        for (let lastLineNumber = generatedPositionForOriginalLine.lastKey; lineNumber <= lastLineNumber; ++lineNumber) {
            let generatedPosition = generatedPositionForOriginalLine.get(lineNumber);
            if (generatedPosition)
                return generatedPosition;
        }
        return generatedPositionForOriginalLine.firstValue;
    }
};

WI.SourceMap._instances = new IterableWeakSet;

WI.SourceMap._VLQ = class VLQ
{
    constructor(context, string)
    {
        this._context = context;

        this._string = string;
        this._position = 0;

        if (!WI.SourceMap._VLQ._base64Map) {
            const base64Digits = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
            WI.SourceMap._VLQ._base64Map = {};
            for (let i = 0; i < base64Digits.length; ++i)
                WI.SourceMap._VLQ._base64Map[base64Digits[i]] = i;
        }
    }

    hasNextCharacter()
    {
        return this._position < this._string.length;
    }

    peekNextCharacter()
    {
        return this._string[this._position];
    }

    takeNextCharacter()
    {
        return this._string[this._position++];
    }

    atSeparator()
    {
        let char = this.peekNextCharacter();
        return char === "," || char === ";";;
    }

    decode()
    {
        // https://tc39.es/ecma426/#sec-base64-vlq

        if (!this.hasNextCharacter() || this.atSeparator())
            return WI.SourceMap._VLQ.AtSeparator;

        let base64Map = WI.SourceMap._VLQ._base64Map;

        let char = this.takeNextCharacter();
        if (!(char in base64Map))
            throw this._context;

        let byte = base64Map[char];
        let sign = byte & 0x01 ? -1 : 1;
        let value = (byte >> 1) & 0x0F;
        let shift = 16;
        while (byte & 0x20) {
            if (!this.hasNextCharacter() || this.atSeparator())
                throw this._context;

            char = this.takeNextCharacter();
            if (!(char in base64Map))
                throw this._context;

            byte = base64Map[char];
            let chunk = byte & 0x1F;
            value += chunk * shift;
            if (value >= 2_147_483_648)
                throw this._context;
            shift *= 32;
        }

        if (value === 0 && sign === -1)
            return -2_147_483_648;

        return value * sign;
    }
};

WI.SourceMap._VLQ.AtSeparator = Symbol("separator");
