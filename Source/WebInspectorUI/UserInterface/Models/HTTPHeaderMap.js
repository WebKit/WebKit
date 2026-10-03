/*
 * Copyright (C) 2026 Devin Rousso <webkit@devinrousso.com>. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

WI.HTTPHeaderMap = class HTTPHeaderMap
{
    constructor(headers)
    {
        this._commonHeaders = [];
        this._uncommonHeaders = [];

        if (headers) {
            for (let item of (headers instanceof WI.HTTPHeaderMap || Array.isArray(headers) ? headers : Object.entries(headers))) {
                let name = item.name ?? item[0];
                let value = item.value ?? item[1];
                let commonName = WI.HTTPHeaderMap._commonHeaderNames.get(name.toLowerCase());
                if (commonName)
                    this.add(commonName, value);
                else
                    this._uncommonHeaders.push({name, value});
            }
        }
    }

    // Public

    get size()
    {
        return this._commonHeaders.length + this._uncommonHeaders.length;
    }

    get(name)
    {
        return this.getAll(name).join(", ");
    }

    getAll(name)
    {
        console.assert(Object.values(WI.HTTPHeader).includes(name), name);

        let result = [];
        for (let header of this._commonHeaders) {
            if (header.name === name)
                result.push(header.value);
        }
        return result;
    }

    has(name)
    {
        console.assert(Object.values(WI.HTTPHeader).includes(name), name);

        return this._commonHeaders.some((header) => header.name === name);
    }

    add(name, value)
    {
        console.assert(Object.values(WI.HTTPHeader).includes(name), name);

        this._commonHeaders.push({name, value});
        return this;
    }

    delete(name)
    {
        console.assert(Object.values(WI.HTTPHeader).includes(name), name);

        let length = this._commonHeaders.length;
        this._commonHeaders = this._commonHeaders.filter((header) => header.name !== name);
        return length !== this._commonHeaders.length;
    }

    clear()
    {
        this._commonHeaders = [];
        this._uncommonHeaders = [];
    }

    copy()
    {
        return new WI.HTTPHeaderMap(this);
    }

    combined()
    {
        let result = new Map;
        let keys = new Map;
        for (let [name, value] of this) {
            let key = keys.getOrInsert(name.toLowerCase(), name);
            result.set(key, result.has(key) ? result.get(key) + ", " + value : value);
        }
        return result;
    }

    *keys()
    {
        for (let {name} of this._commonHeaders)
            yield name;
        for (let {name} of this._uncommonHeaders)
            yield name;
    }

    *values()
    {
        for (let {value} of this._commonHeaders)
            yield value;
        for (let {value} of this._uncommonHeaders)
            yield value;
    }

    *[Symbol.iterator]()
    {
        for (let {name, value} of this._commonHeaders)
            yield [name, value];
        for (let {name, value} of this._uncommonHeaders)
            yield [name, value];
    }

    toJSON()
    {
        return [...this._commonHeaders, ...this._uncommonHeaders];
    }

    // Testing

    getCombinedUncommonHeaderValueForTesting(name)
    {
        return this.getOriginalUncommonHeaderValuesForTesting(name).join(", ");
    }

    getOriginalUncommonHeaderValuesForTesting(name)
    {
        let lowerCaseName = name.toLowerCase();
        console.assert(!WI.HTTPHeaderMap._commonHeaderNames.has(lowerCaseName), name);

        let result = [];
        for (let header of this._uncommonHeaders) {
            if (header.name.toLowerCase() === lowerCaseName)
                result.push(header.value);
        }
        return result;
    }
};

WI.HTTPHeader = {
    Authorization: "Authorization",
    ContentEncoding: "Content-Encoding",
    ContentLength: "Content-Length",
    ContentType: "Content-Type",
    Cookie: "Cookie",
    Location: "Location",
    Range: "Range",
    Referer: "Referer",
    ServerTiming: "Server-Timing",
    SetCookie: "Set-Cookie",
};

WI.HTTPHeaderMap._commonHeaderNames = new Map(Object.values(WI.HTTPHeader).map((name) => [name.toLowerCase(), name]));
