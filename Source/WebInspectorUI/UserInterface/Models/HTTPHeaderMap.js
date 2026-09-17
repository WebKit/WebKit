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
        this._headers = [];

        if (headers instanceof WI.HTTPHeaderMap) {
            for (let [name, value] of headers)
                this.add(name, value);
        } else if (headers) {
            for (let item of (Array.isArray(headers) ? headers : Object.entries(headers)))
                this.add(item.name ?? item[0], item.value ?? item[1]);
        }
    }

    // Public

    get size()
    {
        return this._headers.length;
    }

    get length()
    {
        return this._headers.length;
    }

    get(name)
    {
        return this.getAll(name).join(", ");
    }

    getAll(name)
    {
        let lowerCaseName = name.toLowerCase();
        let result = [];
        for (let header of this._headers) {
            if (header.name.toLowerCase() === lowerCaseName)
                result.push(header.value);
        }
        return result;
    }

    has(name)
    {
        let lowerCaseName = name.toLowerCase();
        return this._headers.some((header) => header.name.toLowerCase() === lowerCaseName);
    }

    add(name, value)
    {
        this._headers.push({name, value});
        return this;
    }

    delete(name)
    {
        let length = this._headers.length;
        let lowerCaseName = name.toLowerCase();
        this._headers = this._headers.filter((header) => header.name.toLowerCase() !== lowerCaseName);
        return length !== this._headers.length;
    }

    clear()
    {
        this._headers = [];
    }

    copy()
    {
        return new WI.HTTPHeaderMap(this);
    }

    combined()
    {
        let result = new Map;
        let keys = new Map;
        for (let {name, value} of this._headers) {
            let key = keys.getOrInsert(name.toLowerCase(), name);
            result.set(key, result.has(key) ? result.get(key) + ", " + value : value);
        }
        return result;
    }

    *keys()
    {
        for (let {name} of this._headers)
            yield name;
    }

    *values()
    {
        for (let {value} of this._headers)
            yield value;
    }

    *[Symbol.iterator]()
    {
        for (let {name, value} of this._headers)
            yield [name, value];
    }

    toJSON()
    {
        return this._headers;
    }
};

WI.HTTPHeader = {
    Authorization: "authorization",
    ContentEncoding: "content-encoding",
    ContentLength: "content-length",
    ContentType: "content-type",
    Cookie: "cookie",
    Location: "location",
    Range: "range",
    Referer: "referer",
    ServerTiming: "server-Timing",
    SetCookie: "set-cookie",
};
