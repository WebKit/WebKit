/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
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

var base = base || {};

(function(){

base.asInteger = function(stringOrInteger)
{
    if (typeof stringOrInteger == 'string')
        return parseInt(stringOrInteger);
    return stringOrInteger;
};

base.endsWith = function(string, suffix)
{
    if (suffix.length > string.length)
        return false;
    var expectedIndex = string.length - suffix.length;
    return string.lastIndexOf(suffix) == expectedIndex;
};

base.repeatString = function(string, count)
{
    return new Array(count + 1).join(string);
};

base.joinPath = function(parent, child)
{
    if (parent.length == 0)
        return child;
    return parent + '/' + child;
};

base.trimExtension = function(url)
{
    var index = url.lastIndexOf('.');
    if (index == -1)
        return url;
    return url.substr(0, index);
}

base.uniquifyArray = function(array)
{
    var seen = {};
    var result = [];
    $.each(array, function(index, value) {
        if (seen[value])
            return;
        seen[value] = true;
        result.push(value);
    });
    return result;
};

base.keys = function(dictionary)
{
    var keys = [];
    $.each(dictionary, function(key, value) {
        keys.push(key);
    });
    return keys;
};

base.filterTree = function(tree, isLeaf, predicate)
{
    var filteredTree = {};

    function walkSubtree(subtree, directory)
    {
        for (var childName in subtree) {
            var child = subtree[childName];
            var childPath = base.joinPath(directory, childName);
            if (isLeaf(child)) {
                if (predicate(child))
                    filteredTree[childPath] = child;
                continue;
            }
            walkSubtree(child, childPath);
        }
    }

    walkSubtree(tree, '');
    return filteredTree;
};

base.RequestTracker = function(requestsInFlight, callback, args)
{
    this._requestsInFlight = requestsInFlight;
    this._callback = callback;
    this._args = args || [];
};

base.RequestTracker.prototype.requestComplete = function()
{
    --this._requestsInFlight;
    if (!this._requestsInFlight)
        this._callback.apply(null, this._args);
};

base.callInParallel = function(functionList, callback)
{
    var requestTracker = new base.RequestTracker(functionList.length, callback);

    $.each(functionList, function(index, func) {
        func(function() {
            requestTracker.requestComplete();
        });
    });
};

base.callInSequence = function(func, argumentList, callback)
{
    var nextIndex = 0;

    function callNext()
    {
        if (nextIndex >= argumentList.length) {
            callback();
            return;
        }

        func(argumentList[nextIndex++], callNext);
    }

    callNext();
};

base.CallbackIterator = function(callback, listOfArgumentArrays)
{
    this._callback = callback;
    this._nextIndex = 0;
    this._listOfArgumentArrays = listOfArgumentArrays;
};

base.CallbackIterator.prototype.hasNext = function()
{
    return this._nextIndex < this._listOfArgumentArrays.length;
};

base.CallbackIterator.prototype.hasPrevious = function()
{
    return this._nextIndex - 2 >= 0;
};

base.CallbackIterator.prototype.callNext = function()
{
    if (!this.hasNext())
        return;
    var args = this._listOfArgumentArrays[this._nextIndex];
    this._nextIndex++;
    this._callback.apply(null, args);
};

base.CallbackIterator.prototype.callPrevious = function()
{
    if (!this.hasPrevious())
        return;
    var args = this._listOfArgumentArrays[this._nextIndex - 2];
    this._nextIndex--;
    this._callback.apply(null, args);
};

base.AsynchronousCache = function(fetch)
{
    this._fetch = fetch;
    this._dataCache = {};
    this._callbackCache = {};
};

base.AsynchronousCache.prototype.get = function(key, callback)
{
    var self = this;

    if (self._dataCache[key]) {
        // FIXME: Consider always calling callback asynchronously.
        callback(self._dataCache[key]);
        return;
    }

    if (key in self._callbackCache) {
        self._callbackCache[key].push(callback);
        return;
    }

    self._callbackCache[key] = [callback];

    self._fetch.call(null, key, function(data) {
        self._dataCache[key] = data;

        var callbackList = self._callbackCache[key];
        delete self._callbackCache[key];

        callbackList.forEach(function(cachedCallback) {
            cachedCallback(data);
        });
    });
};

// Based on http://src.chromium.org/viewvc/chrome/trunk/src/chrome/browser/resources/shared/js/cr/ui.js
base.extends = function(base, prototype)
{
    var extended = function() {
        var element = typeof base == 'string' ? document.createElement(base) : base.call(this);
        extended.prototype.__proto__ = element.__proto__;
        element.__proto__ = extended.prototype;
        element.init && element.init.apply(element, arguments);
        return element;
    }

    extended.prototype = prototype;
    return extended;
}

})();
