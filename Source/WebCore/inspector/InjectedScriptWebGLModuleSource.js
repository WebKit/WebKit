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
 * @param {InjectedScriptHost} InjectedScriptHost
 * @param {Window} inspectedWindow
 * @param {number} injectedScriptId
 */
(function (InjectedScriptHost, inspectedWindow, injectedScriptId) {

/**
 * @constructor
 */
function Cache()
{
    this.reset();
}

Cache.prototype = {
    /**
     * @return {number}
     */
    size: function()
    {
        return this._size;
    },

    reset: function()
    {
        this._items = Object.create(null);
        this._size = 0;
    },

    /**
     * @param {number} key
     * @return {boolean}
     */
    has: function(key)
    {
        return key in this._items;
    },

    /**
     * @param {number} key
     * @return {Object}
     */
    get: function(key)
    {
        return this._items[key];
    },

    /**
     * @param {number} key
     * @param {Object} item
     */
    put: function(key, item)
    {
        if (!this.has(key))
            ++this._size;
        this._items[key] = item;
    }
}

/**
 * @constructor
 * @param {Resource|Object} thisObject
 * @param {string} functionName
 * @param {Array|Arguments} args
 * @param {Resource|*} result
 */
function Call(thisObject, functionName, args, result)
{
    this._thisObject = thisObject;
    this._functionName = functionName;
    this._args = Array.prototype.slice.call(args, 0);
    this._result = result;
}

Call.prototype = {
    /**
     * @return {Resource}
     */
    resource: function()
    {
        return Resource.forObject(this._thisObject);
    },

    /**
     * @return {string}
     */
    functionName: function()
    {
        return this._functionName;
    },

    /**
     * @return {Array}
     */
    args: function()
    {
        return this._args;
    },

    /**
     * @return {*}
     */
    result: function()
    {
        return this._result;
    }
}

/**
 * @constructor
 * @param {Object} wrappedObject
 */
function Resource(wrappedObject)
{
    this._id = ++Resource._uniqueId;
    this._resourceManager = null;
    this.setWrappedObject(wrappedObject);
}

Resource._uniqueId = 0;

/**
 * @param {Object} obj
 * @return {Resource}
 */
Resource.forObject = function(obj)
{
    if (!obj || obj instanceof Resource)
        return obj;
    if (typeof obj === "object")
        return obj["__resourceObject"];
    return null;
}

Resource.prototype = {
    /**
     * @return {number}
     */
    id: function()
    {
        return this._id;
    },

    /**
     * @return {Object}
     */
    wrappedObject: function()
    {
        return this._wrappedObject;
    },

    /**
     * @param {Object} value
     */
    setWrappedObject: function(value)
    {
        console.assert(value && !(value instanceof Resource), "Binding a Resource object to another Resource object?");
        this._wrappedObject = value;
        this._bindObjectToResource(value);
    },

    /**
     * @return {Object}
     */
    proxyObject: function()
    {
        // No proxy wrapping by default.
        return this.wrappedObject();
    },

    /**
     * @return {ResourceTrackingManager}
     */
    manager: function()
    {
        return this._resourceManager;
    },

    /**
     * @param {ResourceTrackingManager} value
     */
    setManager: function(value)
    {
        this._resourceManager = value;
    },

    /**
     * @param {Object} object
     */
    _bindObjectToResource: function(object)
    {
        object["__resourceObject"] = this;
    }
}

/**
 * @constructor
 * @extends {Resource}
 * @param {WebGLRenderingContext} glContext
 */
function WebGLRenderingContextResource(glContext)
{
    Resource.call(this, glContext);
    this._proxyObject = null;
}

WebGLRenderingContextResource.prototype = {
    /**
     * @return {Object}
     */
    proxyObject: function()
    {
        if (!this._proxyObject)
            this._proxyObject = this._wrapObject();
        return this._proxyObject;
    },

    /**
     * @return {Object}
     */
    _wrapObject: function()
    {
        var glContext = this.wrappedObject();
        var proxy = Object.create(glContext.__proto__); // In order to emulate "instanceof".

        var self = this;
        function processProperty(property)
        {
            if (typeof glContext[property] === "function") {
                // FIXME: override GL calls affecting resources states here.
                proxy[property] = self._wrapFunction(self, glContext, glContext[property], property);
            } else if (/^[A-Z0-9_]+$/.test(property)) {
                // Fast access to enums and constants.
                proxy[property] = glContext[property];
            } else {
                Object.defineProperty(proxy, property, {
                    get: function()
                    {
                        return glContext[property];
                    },
                    set: function(value)
                    {
                        glContext[property] = value;
                    }
                });
            }
        }

        for (var property in glContext)
            processProperty(property);

        return proxy;
    },

    /**
     * @param {Resource} resource
     * @param {WebGLRenderingContext} originalObject
     * @param {Function} originalFunction
     * @param {string} functionName
     * @return {*}
     */
    _wrapFunction: function(resource, originalObject, originalFunction, functionName)
    {
        return function()
        {
            var manager = resource.manager();
            if (!manager || !manager.capturing())
                return originalFunction.apply(originalObject, arguments);
            manager.captureArguments(resource, arguments);
            var result = originalFunction.apply(originalObject, arguments);
            var call = new Call(resource, functionName, arguments, result);
            manager.reportCall(call);
            return result;
        };
    }
}

WebGLRenderingContextResource.prototype.__proto__ = Resource.prototype;

/**
 * @constructor
 * @param {WebGLRenderingContext} originalObject
 * @param {Function} originalFunction
 * @param {string} functionName
 * @param {Array} args
 */
WebGLRenderingContextResource.WrapFunction = function(originalObject, originalFunction, functionName, args)
{
    this._originalObject = originalObject;
    this._originalFunction = originalFunction;
    this._functionName = functionName;
    this._args = args;
    this._glResource = Resource.forObject(originalObject);
}

WebGLRenderingContextResource.WrapFunction.prototype = {
    /**
     * @return {*}
     */
    result: function()
    {
        if (!this._executed) {
            this._executed = true;
            this._result = this._originalFunction.apply(this._originalObject, this._args);
        }
        return this._result;
    },

    /**
     * @return {Call}
     */
    call: function()
    {
        if (!this._call)
            this._call = new Call(this._glResource, this._functionName, this._args, this.result());
        return this._call;
    }
}

/**
 * @constructor
 */
function TraceLog()
{
    this._calls = [];
    this._resourceCache = new Cache();
}

TraceLog.prototype = {
    /**
     * @return {number}
     */
    size: function()
    {
        return this._calls.length;
    },

    /**
     * @param {Resource} resource
     */
    captureResource: function(resource)
    {
        // FIXME: Capture current resource state to start the replay from.
    },

    /**
     * @param {Call} call
     */
    addCall: function(call)
    {
        // FIXME: Clone call and push the clone.
        this._calls.push(call);
    }
}

/**
 * @constructor
 */
function ResourceTrackingManager()
{
    this._capturing = false;
    this._stopCapturingOnFrameEnd = false;
    this._lastTraceLog = null;
}

ResourceTrackingManager.prototype = {
    /**
     * @return {boolean}
     */
    capturing: function()
    {
        return this._capturing;
    },

    /**
     * @return {TraceLog}
     */
    lastTraceLog: function()
    {
        return this._lastTraceLog;
    },

    /**
     * @param {Resource} resource
     */
    registerResource: function(resource)
    {
        resource.setManager(this);
    },

    startCapturing: function()
    {
        if (!this._capturing)
            this._lastTraceLog = new TraceLog();
        this._capturing = true;
        this._stopCapturingOnFrameEnd = false;
    },

    /**
     * @param {TraceLog=} traceLog
     */
    stopCapturing: function(traceLog)
    {
        if (traceLog && this._lastTraceLog !== traceLog)
            return;
        this._capturing = false;
        this._stopCapturingOnFrameEnd = false;
    },

    captureFrame: function()
    {
        this._lastTraceLog = new TraceLog();
        this._capturing = true;
        this._stopCapturingOnFrameEnd = true;
    },

    captureArguments: function(resource, args)
    {
        if (!this._capturing)
            return;
        this._lastTraceLog.captureResource(resource);
        for (var i = 0, n = args.length; i < n; ++i) {
            var res = Resource.forObject(args[i]);
            if (res)
                this._lastTraceLog.captureResource(res);
        }
    },

    /**
     * @param {Call} call
     */
    reportCall: function(call)
    {
        if (!this._capturing)
            return;
        this._lastTraceLog.addCall(call);
        if (this._stopCapturingOnFrameEnd && this._lastTraceLog.size() === 1) {
            this._stopCapturingOnFrameEnd = false;
            this._setZeroTimeouts(this.stopCapturing.bind(this, this._lastTraceLog));
        }
    },

    /**
     * @param {Function} callback
     */
    _setZeroTimeouts: function(callback)
    {
        // We need a fastest async callback, whatever fires first.
        // Usually a postMessage should be faster than a setTimeout(0).
        var channel = new MessageChannel();
        channel.port1.onmessage = callback;
        channel.port2.postMessage("");
        inspectedWindow.setTimeout(callback, 0);
    }
}

/**
 * @constructor
 */
var InjectedScript = function()
{
    this._manager = new ResourceTrackingManager();
}

InjectedScript.prototype = {
    /**
     * @param {WebGLRenderingContext} glContext
     * @return {Object}
     */
    wrapWebGLContext: function(glContext)
    {
        var resource = Resource.forObject(glContext) || new WebGLRenderingContextResource(glContext);
        this._manager.registerResource(resource);
        var proxy = resource.proxyObject();
        return proxy;
    },

    captureFrame: function()
    {
        this._manager.captureFrame();
    }
}

var injectedScript = new InjectedScript();
return injectedScript;

})
