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

// FIXME: <https://webkit.org/b/165155> Web Inspector: Use URL constructor to better handle all kinds of URLs

function removeURLFragment(url)
{
    var hashIndex = url.indexOf("#");
    if (hashIndex >= 0)
        return url.substring(0, hashIndex);
    return url;
}

function relativePath(path, basePath)
{
    console.assert(path.charAt(0) === "/");
    console.assert(basePath.charAt(0) === "/");

    var pathComponents = path.split("/");
    var baseComponents = basePath.replace(/\/$/, "").split("/");
    var finalComponents = [];

    var index = 1;
    for (; index < pathComponents.length && index < baseComponents.length; ++index) {
        if (pathComponents[index] !== baseComponents[index])
            break;
    }

    for (var i = index; i < baseComponents.length; ++i)
        finalComponents.push("..");

    for (var i = index; i < pathComponents.length; ++i)
        finalComponents.push(pathComponents[i]);

    return finalComponents.join("/");
}

function parseSecurityOrigin(securityOrigin)
{
    securityOrigin = securityOrigin ? securityOrigin.trim() : "";

    let match = securityOrigin.match(/^(?<scheme>[^:]+):\/\/(?<host>[^\/:]*)(?::(?<port>[\d]+))?$/i);
    if (!match)
        return {scheme: null, host: null, port: null};

    let scheme = match.groups.scheme.toLowerCase();
    let host = match.groups.host.toLowerCase();
    let port = Number(match.groups.port) || null;

    return {scheme, host, port};
}

function parseDataURL(url)
{
    if (!url.startsWith("data:"))
        return null;

    // data:[<media type>][;charset=<character set>][;base64],<data>
    let match = url.match(/^data:(?<mime>[^;,]*)?(?:;charset=(?<charset>[^;,]*?))?(?<base64>;base64)?,(?<data>.*)$/);
    if (!match)
        return null;

    let scheme = "data";
    let mimeType = match.groups.mime || "text/plain";
    let charset = match.groups.charset || "US-ASCII";
    let base64 = !!match.groups.base64;
    let data = decodeURIComponent(match.groups.data);

    return {scheme, mimeType, charset, base64, data};
}

function parseURL(url)
{
    url = url ? url.trim() : "";

    if (url.startsWith("data:"))
        return {scheme: "data", userinfo: null, host: null, port: null, path: null, queryString: null, fragment: null, lastPathComponent: null};

    let match = url.match(/^(?<scheme>[^\/:]+):\/\/(?:(?<userinfo>[^#@\/]+)@)?(?<host>[^\/#:]*)(?::(?<port>[\d]+))?(?:(?<path>\/[^#]*)?(?:#(?<fragment>.*))?)?$/i);
    if (!match)
        return {scheme: null, userinfo: null, host: null, port: null, path: null, queryString: null, fragment: null, lastPathComponent: null};

    let scheme = match.groups.scheme.toLowerCase();
    let userinfo = match.groups.userinfo || null;
    let host = match.groups.host.toLowerCase();
    let port = Number(match.groups.port) || null;
    let wholePath = match.groups.path || null;
    let fragment = match.groups.fragment || null;
    let path = wholePath;
    let queryString = null;

    // Split the path and the query string.
    if (wholePath) {
        let indexOfQuery = wholePath.indexOf("?");
        if (indexOfQuery !== -1) {
            path = wholePath.substring(0, indexOfQuery);
            queryString = wholePath.substring(indexOfQuery + 1);
        }
        path = resolveDotsInPath(path);
    }

    // Find last path component.
    let lastPathComponent = null;
    if (path && path !== "/") {
        // Skip the trailing slash if there is one.
        let endOffset = path[path.length - 1] === "/" ? 1 : 0;
        let lastSlashIndex = path.lastIndexOf("/", path.length - 1 - endOffset);
        if (lastSlashIndex !== -1)
            lastPathComponent = path.substring(lastSlashIndex + 1, path.length - endOffset);
    }

    return {scheme, userinfo, host, port, path, queryString, fragment, lastPathComponent};
}

function absoluteURL(partialURL, baseURL)
{
    partialURL = partialURL ? partialURL.trim() : "";

    // Return data and javascript URLs as-is.
    if (partialURL.startsWith("data:") || partialURL.startsWith("javascript:") || partialURL.startsWith("mailto:"))
        return partialURL;

    // If the URL has a scheme it is already a full URL, so return it.
    if (parseURL(partialURL).scheme)
        return partialURL;

    // If there is no partial URL, just return the base URL.
    if (!partialURL)
        return baseURL || null;

    var baseURLComponents = parseURL(baseURL);

    // The base URL needs to be an absolute URL. Return null if it isn't.
    if (!baseURLComponents.scheme)
        return null;

    // A URL that starts with "//" is a full URL without the scheme. Use the base URL scheme.
    if (partialURL[0] === "/" && partialURL[1] === "/")
        return baseURLComponents.scheme + ":" + partialURL;

    // The path can be null for URLs that have just a scheme and host (like "http://apple.com"). So make the path be "/".
    if (!baseURLComponents.path)
        baseURLComponents.path = "/";

    // Generate the base URL prefix that is used in the rest of the cases.
    var baseURLPrefix = baseURLComponents.scheme + "://" + baseURLComponents.host + (baseURLComponents.port ? (":" + baseURLComponents.port) : "");

    // A URL that starts with "?" is just a query string that gets applied to the base URL (replacing the base URL query string and fragment).
    if (partialURL[0] === "?")
        return baseURLPrefix + baseURLComponents.path + partialURL;

    // A URL that starts with "/" is an absolute path that gets applied to the base URL (replacing the base URL path, query string and fragment).
    if (partialURL[0] === "/")
        return baseURLPrefix + resolveDotsInPath(partialURL);

    // A URL that starts with "#" is just a fragment that gets applied to the base URL (replacing the base URL fragment, maintaining the query string).
    if (partialURL[0] === "#") {
        let queryStringComponent = baseURLComponents.queryString ? "?" + baseURLComponents.queryString : "";
        return baseURLPrefix + baseURLComponents.path + queryStringComponent + partialURL;
    }

    // Generate the base path that is used in the final case by removing everything after the last "/" from the base URL's path.
    var basePath = baseURLComponents.path.substring(0, baseURLComponents.path.lastIndexOf("/")) + "/";
    return baseURLPrefix + resolveDotsInPath(basePath + partialURL);
}

function parseQueryString(queryString, arrayResult)
{
    if (!queryString)
        return arrayResult ? [] : {};

    function decode(string)
    {
        try {
            // Replace "+" with " " then decode percent encoded values.
            return decodeURIComponent(string.replace(/\+/g, " "));
        } catch {
            return string;
        }
    }

    var parameters = arrayResult ? [] : {};
    var parameterStrings = queryString.split("&");
    for (var i = 0; i < parameterStrings.length; ++i) {
        var pair = parameterStrings[i].split("=").map(decode);
        if (arrayResult)
            parameters.push({name: pair[0], value: pair[1]});
        else
            parameters[pair[0]] = pair[1];
    }

    return parameters;
}

WI.displayNameForURL = function(url, urlComponents)
{
    if (url.startsWith("data:"))
        return WI.truncateURL(url);

    if (!urlComponents)
        urlComponents = parseURL(url);

    var displayName;
    try {
        displayName = decodeURIComponent(urlComponents.lastPathComponent || "");
    } catch {
        displayName = urlComponents.lastPathComponent;
    }

    return displayName || WI.displayNameForHost(urlComponents.host) || url;
};

WI.truncateURL = function(url, multiline = false, dataURIMaxSize = 6)
{
    if (!url.startsWith("data:"))
        return url;

    const dataIndex = url.indexOf(",") + 1;
    let header = url.slice(0, dataIndex);
    if (multiline)
        header += "\n";

    const data = url.slice(dataIndex);
    if (data.length < dataURIMaxSize)
        return header + data;

    const firstChunk = data.slice(0, Math.ceil(dataURIMaxSize / 2));
    const ellipsis = "\u2026";
    const middleChunk = multiline ? `\n${ellipsis}\n` : ellipsis;
    const lastChunk = data.slice(-Math.floor(dataURIMaxSize / 2));
    return header + firstChunk + middleChunk + lastChunk;
};

WI.displayNameForHost = function(host)
{
    // FIXME <rdar://problem/11237413>: This should decode punycode hostnames.
    return host;
};

// https://tools.ietf.org/html/rfc7540#section-8.1.2.3
WI.h2Authority = function(components)
{
    let {scheme, userinfo, host, port} = components;
    let result = host || "";

    // The authority MUST NOT include the deprecated "userinfo"
    // subcomponent for "http" or "https" schemed URIs.
    if (userinfo && (scheme !== "http" && scheme !== "https"))
        result = userinfo + "@" + result;
    if (port)
        result += ":" + port;

    return result;
};

// https://tools.ietf.org/html/rfc7540#section-8.1.2.3
WI.h2Path = function(components)
{
    let {scheme, path, queryString} = components;
    let result = path || "";

    // The ":path" pseudo-header field includes the path and query parts
    // of the target URI. [...] This pseudo-header field MUST NOT be empty
    // for "http" or "https" URIs; "http" or "https" URIs that do not contain
    // a path component MUST include a value of '/'.
    if (!path && (scheme === "http" || scheme === "https"))
        result = "/";
    if (queryString)
        result += "?" + queryString;

    return result;
};
