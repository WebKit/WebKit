/*
 * Copyright (C) 2017 Apple Inc. All rights reserved.
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

WI.Cookie = class Cookie
{
    constructor(type, name, value, raw, expires, maxAge, path, domain, secure, httpOnly)
    {
        console.assert(Object.values(WI.Cookie.Type).includes(type));
        console.assert(typeof name === "string");
        console.assert(typeof value === "string");
        console.assert(!raw || typeof raw === "string");
        console.assert(!expires || expires instanceof Date);
        console.assert(!maxAge || typeof maxAge === "number");
        console.assert(!path || typeof path === "string");
        console.assert(!domain || typeof domain === "string");
        console.assert(!secure || typeof secure === "boolean");
        console.assert(!httpOnly || typeof httpOnly === "boolean");

        this.type = type;
        this.name = name || "";
        this.value = value || "";

        if (this.type === WI.Cookie.Type.Response) {
            this.rawHeader = raw || "";
            this.expires = expires || null;
            this.maxAge = maxAge || null;
            this.path = path || null;
            this.domain = domain || null;
            this.secure = secure || false;
            this.httpOnly = httpOnly || false;
        }
    }

    // Public

    expirationDate(requestSentDate)
    {
        if (this.maxAge) {
            let startDate = requestSentDate || new Date;
            return new Date(startDate.getTime() + (this.maxAge * 1000));
        }

        return this.expires;
    }

    // Static

    // RFC 6265 defines the HTTP Cookie and Set-Cookie header fields:
    // https://www.ietf.org/rfc/rfc6265.txt

    static parseCookieRequestHeader(header)
    {
        if (!header)
            return [];

        header = header.trim();
        if (!header)
            return [];

        let cookies = [];

        // Cookie: <name> = <value> ( ";" SP <name> = <value> )*?
        // NOTE: Just name/value pairs.

        let pairs = header.split(/; /);
        for (let pair of pairs) {
            let match = pair.match(/^(?<name>[^\s=]+)[ \t]*=[ \t]*(?<value>.*)$/);
            if (!match) {
                WI.reportInternalError("Failed to parse Cookie pair", {header, pair});
                continue;
            }

            let {name, value} = match.groups;
            cookies.push(new WI.Cookie(WI.Cookie.Type.Request, name, value));
        }

        return cookies;
    }

    static parseSetCookieResponseHeader(header)
    {
        if (!header)
            return null;

        // Set-Cookie: <name> = <value> ( ";" SP <attr-maybe-pair> )*?
        // NOTE: Some attributes can have pairs (e.g. "Path=/"), some are only a
        // single word (e.g. "Secure").

        // Parse name/value.
        let nameValueMatch = header.match(/^(?<name>[^\s=]+)[ \t]*=[ \t]*(?<value>[^;]*)/);
        if (!nameValueMatch) {
            WI.reportInternalError("Failed to parse Set-Cookie header", {header});
            return null;
        }

        let {name, value} = nameValueMatch.groups;
        let expires = null;
        let maxAge = null;
        let path = null;
        let domain = null;
        let secure = false;
        let httpOnly = false;

        // Parse Attributes
        let remaining = header.substr(nameValueMatch[0].length);
        let attributes = remaining.split(/; ?/);
        for (let attribute of attributes) {
            if (!attribute)
                continue;

            let match = attribute.match(/^(?<name>[^\s=]+)(?:=(?<value>.*))?$/);
            if (!match) {
                console.error("Failed to parse Set-Cookie attribute:", attribute);
                continue;
            }

            let attributeName = match.groups.name;
            let attributeValue = match.groups.value;
            switch (attributeName.toLowerCase()) {
            case "expires":
                console.assert(attributeValue);
                expires = new Date(attributeValue);
                if (isNaN(expires.getTime())) {
                    console.warn("Invalid Expires date:", attributeValue);
                    expires = null;
                }
                break;
            case "max-age":
                console.assert(attributeValue);
                maxAge = parseInt(attributeValue, 10);
                if (isNaN(maxAge) || !/^\d+$/.test(attributeValue)) {
                    console.warn("Invalid MaxAge value:", attributeValue);
                    maxAge = null;
                }
                break;
            case "path":
                console.assert(attributeValue);
                path = attributeValue;
                break;
            case "domain":
                console.assert(attributeValue);
                domain = attributeValue;
                break;
            case "secure":
                console.assert(!attributeValue);
                secure = true;
                break;
            case "httponly":
                console.assert(!attributeValue);
                httpOnly = true;
                break;
            default:
                console.warn("Unknown Cookie attribute:", attribute);
                break;
            }
        }

        return new WI.Cookie(WI.Cookie.Type.Response, name, value, header, expires, maxAge, path, domain, secure, httpOnly);
    }
}

WI.Cookie.Type = {
    Request: "request",
    Response: "response",
};
