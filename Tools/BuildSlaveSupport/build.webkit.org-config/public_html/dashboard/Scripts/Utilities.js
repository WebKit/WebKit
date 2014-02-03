/*
 * Copyright (C) 2013, 2014 Apple Inc. All rights reserved.
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

JSON.LoadError = "JSONLoadError";
JSON.ParseError = "JSONParseError";

// JSON.load() may be called using the following forms:
//
// JSON.load("http://www.apple.com", function() {/* success */})
// OR
// JSON.load("http://www.apple.com", function() {/* success */}, {withCredentials: true, ...})
// OR
// JSON.load("http://www.apple.com", function() {/* success */}, function() {/* failure */})
// OR
// JSON.load("http://www.apple.com", function() {/* success */}, function() {/* failure */}, {withCredentials: true, ...})
JSON.load = function(url, successCallback, failureCallback, options)
{
    console.assert(url);

    if (!(successCallback instanceof Function))
        return;

    if (failureCallback && typeof failureCallback === "object")
        options = failureCallback;

    if (!(failureCallback instanceof Function))
        failureCallback = function() { };

    if (typeof options !== "object")
        options = {};

    var request = new XMLHttpRequest;
    request.onreadystatechange = function() {
        if (this.readyState !== 4)
            return;

        // Don't consider a status of 0 to be a load error for easier testing with local files.
        var loadErrorOccurred = this.status !== 0 && this.status !== 200;
        if (loadErrorOccurred) {
            failureCallback({errorType: JSON.LoadError, error: this.statusText, errorHTTPCode: this.status});
            return;
        }

        try {
            var responseText = request.responseText;
            if (options.hasOwnProperty("jsonpCallbackName"))
                responseText = responseText.replace(new RegExp("^" + options.jsonpCallbackName + "\\((.*)\\);?$"), "$1");
            var data = JSON.parse(responseText);
        } catch (e) {
            var data = {errorType: JSON.ParseError, error: e.message};
            failureCallback(data);
            return;
        }

        successCallback(data);
    };

    request.open("GET", url);
    if (options.hasOwnProperty("withCredentials"))
        request.withCredentials = options.withCredentials;
    request.send();
};

function loadXML(url, callback, options) {
    console.assert(url);

    if (!(callback instanceof Function))
        return;

    var request = new XMLHttpRequest;
    request.onreadystatechange = function() {
        if (this.readyState !== 4)
            return;

        // Allow a status of 0 for easier testing with local files.
        if (!this.status || this.status === 200)
            callback(request.responseXML);
    };

    request.open("GET", url);
    if ((typeof options === "object") && options.hasOwnProperty("withCredentials"))
        request.withCredentials = options.withCredentials;
    request.send();
};

Node.prototype.isAncestor = function(node)
{
    if (!node)
        return false;

    var currentNode = node.parentNode;
    while (currentNode) {
        if (this === currentNode)
            return true;
        currentNode = currentNode.parentNode;
    }

    return false;
}

Node.prototype.isDescendant = function(descendant)
{
    return !!descendant && descendant.isAncestor(this);
}

Node.prototype.isSelfOrAncestor = function(node)
{
    return !!node && (node === this || this.isAncestor(node));
}

Node.prototype.isSelfOrDescendant = function(node)
{
    return !!node && (node === this || this.isDescendant(node));
}

Element.prototype.removeChildren = function()
{
    // This has been tested to be the fastest removal method.
    if (this.firstChild)
        this.textContent = "";
};

DOMTokenList.prototype.contains = function(string)
{
    for (var i = 0, end = this.length; i < end; ++i) {
        if (this.item(i) === string)
            return true;
    }
    return false;
}

Array.prototype.contains = function(value)
{
    return this.indexOf(value) >= 0;
};

Array.prototype.findFirst = function(predicate)
{
    for (var i = 0; i < this.length; ++i) {
        if (predicate(this[i]))
            return this[i];
    }

    return null;
};

String.prototype.contains = function(substring)
{
    return this.indexOf(substring) >= 0;
};
