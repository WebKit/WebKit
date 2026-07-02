/*
 * Copyright (C) 2024-2026 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    distribution and/or other materials provided with the distribution.
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

WI.ResourceUtilities = class ResourceUtilities
{
    static generateFetchCode(resource)
    {
        console.assert(resource instanceof WI.Resource || resource instanceof WI.Redirect, resource);

        let options = {};

        if (resource.requestData)
            options.body = resource.requestData;

        options.cache = "default";
        options.credentials = (resource.requestCookies.length || resource.requestHeaders.valueForCaseInsensitiveKey("Authorization")) ? "include" : "omit";

        // https://fetch.spec.whatwg.org/#forbidden-header-name
        const forbiddenHeaders = new Set([
            "accept-charset",
            "accept-encoding",
            "access-control-request-headers",
            "access-control-request-method",
            "connection",
            "content-length",
            "cookie",
            "cookie2",
            "date",
            "dnt",
            "expect",
            "host",
            "keep-alive",
            "origin",
            "referer",
            "te",
            "trailer",
            "transfer-encoding",
            "upgrade",
            "via",
        ]);
        let headers = Object.entries(resource.requestHeaders)
            .filter((header) => {
                let key = header[0].toLowerCase();
                if (forbiddenHeaders.has(key))
                    return false;
                if (key.startsWith("proxy-") || key.startsWith("sec-"))
                    return false;
                return true;
            })
            .sort((a, b) => a[0].extendedLocaleCompare(b[0]))
            .reduce((accumulator, current) => {
                accumulator[current[0]] = current[1];
                return accumulator;
            }, {});
        if (!isEmptyObject(headers))
            options.headers = headers;

        if (resource._integrity)
            options.integrity = resource._integrity;

        if (resource.requestMethod)
            options.method = resource.requestMethod;

        options.mode = "cors";
        options.redirect = "follow";

        let referrer = resource.requestHeaders.valueForCaseInsensitiveKey("Referer");
        if (referrer)
            options.referrer = referrer;

        if (resource._referrerPolicy)
            options.referrerPolicy = resource._referrerPolicy;

        return `fetch(${JSON.stringify(resource.url)}, ${JSON.stringify(options, null, WI.indentString())})`;
    }

    static generateCURLCommand(resource)
    {
        console.assert(resource instanceof WI.Resource || resource instanceof WI.Redirect, "Expected WI.Resource or WI.Redirect", resource);

        function escapeStringPosix(str) {
            function escapeCharacter(x) {
                let code = x.charCodeAt(0);
                let hex = code.toString(16);
                if (code < 256)
                    return "\\x" + hex.padStart(2, "0");
                return "\\u" + hex.padStart(4, "0");
            }

            if (/[^\x20-\x7E]|'/.test(str)) {
                // Use ANSI-C quoting syntax.
                return "$'" + str.replace(/\\/g, "\\\\")
                                 .replace(/'/g, "\\'")
                                 .replace(/\n/g, "\\n")
                                 .replace(/\r/g, "\\r")
                                 .replace(/!/g, "\\041")
                                 .replace(/[^\x20-\x7E]/g, escapeCharacter) + "'";
            }

            // Use single quote syntax.
            return "'" + str + "'";
        }

        let command = ["curl " + escapeStringPosix(resource.url).replace(/[[{}\]]/g, "\\$&")];
        command.push("-X " + escapeStringPosix(resource.requestMethod));

        for (let key in resource.requestHeaders)
            command.push("-H " + escapeStringPosix(`${key}: ${resource.requestHeaders[key]}`));

        if (resource.requestDataContentType && resource.requestMethod !== WI.HTTPUtilities.RequestMethod.GET && resource.requestData) {
            if (resource.requestDataContentType.match(/^application\/x-www-form-urlencoded\s*(;.*)?$/i))
                command.push("--data " + escapeStringPosix(resource.requestData));
            else
                command.push("--data-raw " + escapeStringPosix(resource.requestData));
        }

        return command.join(" \\\n");
    }

    static stringifyHTTPRequestHeaders(resource)
    {
        console.assert(resource instanceof WI.Resource || resource instanceof WI.Redirect, "Expected WI.Resource or WI.Redirect", resource);

        let lines = [];

        let protocol = resource.protocol || "";
        if (protocol === "h2") {
            // HTTP/2 Request pseudo headers:
            // https://tools.ietf.org/html/rfc7540#section-8.1.2.3
            lines.push(`:method: ${resource.requestMethod}`);
            lines.push(`:scheme: ${resource.urlComponents.scheme}`);
            lines.push(`:authority: ${WI.h2Authority(resource.urlComponents)}`);
            lines.push(`:path: ${WI.h2Path(resource.urlComponents)}`);
        } else {
            // HTTP/1.1 request line:
            // https://www.w3.org/Protocols/rfc2616/rfc2616-sec5.html#sec5.1
            lines.push(`${resource.requestMethod} ${resource.urlComponents.path}${protocol ? " " + protocol.toUpperCase() : ""}`);
        }

        for (let key in resource.requestHeaders)
            lines.push(`${key}: ${resource.requestHeaders[key]}`);

        return lines.join("\n") + "\n";
    }

    static stringifyHTTPResponseHeaders(resource)
    {
        console.assert(resource instanceof WI.Resource || resource instanceof WI.Redirect, "Expected WI.Resource or WI.Redirect", resource);

        let lines = [];

        // Handle property name differences between Resource and Redirect
        let statusCode = resource.statusCode ?? resource.responseStatusCode;
        let statusText = resource.statusText ?? resource.responseStatusText;

        let protocol = resource.protocol || "";
        if (protocol === "h2") {
            // HTTP/2 Response pseudo headers:
            // https://tools.ietf.org/html/rfc7540#section-8.1.2.4
            lines.push(`:status: ${statusCode}`);
        } else {
            // HTTP/1.1 response status line:
            // https://www.w3.org/Protocols/rfc2616/rfc2616-sec6.html#sec6.1
            lines.push(`${protocol ? protocol.toUpperCase() + " " : ""}${statusCode} ${statusText}`);
        }

        for (let key in resource.responseHeaders)
            lines.push(`${key}: ${resource.responseHeaders[key]}`);

        return lines.join("\n") + "\n";
    }
};
