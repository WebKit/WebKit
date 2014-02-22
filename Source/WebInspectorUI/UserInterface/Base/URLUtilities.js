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

    var match = securityOrigin.match(/^([^:]+):\/\/([^\/:]*)(?::([\d]+))?$/i);
    if (!match)
        return {scheme: null, host: null, port: null};

    var scheme = match[1].toLowerCase();
    var host = match[2].toLowerCase();
    var port = Number(match[3]) || null;

    return {scheme: scheme, host: host, port: port};
}

function parseURL(url)
{
    url = url ? url.trim() : "";

    var match = url.match(/^([^:]+):\/\/([^\/:]*)(?::([\d]+))?(?:(\/[^#]*)(?:#(.*))?)?$/i);
    if (!match)
        return {scheme: null, host: null, port: null, path: null, queryString: null, fragment: null, lastPathComponent: null};

    var scheme = match[1].toLowerCase();
    var host = match[2].toLowerCase();
    var port = Number(match[3]) || null;
    var wholePath = match[4] || null;
    var fragment = match[5] || null;
    var path = wholePath;
    var queryString = null;

    // Split the path and the query string.
    if (wholePath) {
        var indexOfQuery = wholePath.indexOf("?");
        if (indexOfQuery !== -1) {
            path = wholePath.substring(0, indexOfQuery);
            queryString = wholePath.substring(indexOfQuery + 1);
        }
        path = resolveDotsInPath(path);
    }

    // Find last path component.
    var lastPathComponent = null;
    if (path && path !== "/") {
        // Skip the trailing slash if there is one.
        var endOffset = path[path.length - 1] === "/" ? 1 : 0;
        var lastSlashIndex = path.lastIndexOf("/", path.length - 1 - endOffset);
        if (lastSlashIndex !== -1)
            lastPathComponent = path.substring(lastSlashIndex + 1, path.length - endOffset);
    }

    return {scheme: scheme, host: host, port: port, path: path, queryString: queryString, fragment: fragment, lastPathComponent: lastPathComponent};
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

    // Generate the base path that is used in the final case by removing everything after the last "/" from the base URL's path.
    var basePath = baseURLComponents.path.substring(0, baseURLComponents.path.lastIndexOf("/")) + "/";
    return baseURLPrefix + resolveDotsInPath(basePath + partialURL);
}

function parseLocationQueryParameters(arrayResult)
{
    // The first character is always the "?".
    return parseQueryString(window.location.search.substring(1), arrayResult);
}

function parseQueryString(queryString, arrayResult)
{
    if (!queryString)
        return arrayResult ? [] : {};

    function decode(string)
    {
        try {
            // Replace "+" with " " then decode precent encoded values.
            return decodeURIComponent(string.replace(/\+/g, " "));
        } catch (e) {
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

WebInspector.displayNameForURL = function(url, urlComponents)
{
    if (!urlComponents)
        urlComponents = parseURL(url);

    var displayName;
    try {
        displayName = decodeURIComponent(urlComponents.lastPathComponent || "");
    } catch (e) {
        displayName = urlComponents.lastPathComponent;
    }

    return displayName || WebInspector.displayNameForHost(urlComponents.host) || url;
}

WebInspector.displayNameForHost = function(host)
{
    // FIXME <rdar://problem/11237413>: This should decode punycode hostnames.
    return host;
}
