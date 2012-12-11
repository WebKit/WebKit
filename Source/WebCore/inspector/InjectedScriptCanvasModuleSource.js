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

var TypeUtils = {
    /**
     * http://www.khronos.org/registry/typedarray/specs/latest/#7
     * @const
     * @type {!Array.<function(new:ArrayBufferView, ArrayBufferView)>}
     */
    _typedArrayClasses: (function(typeNames) {
        var result = [];
        for (var i = 0, n = typeNames.length; i < n; ++i) {
            if (inspectedWindow[typeNames[i]])
                result.push(inspectedWindow[typeNames[i]]);
        }
        return result;
    })(["Int8Array", "Uint8Array", "Uint8ClampedArray", "Int16Array", "Uint16Array", "Int32Array", "Uint32Array", "Float32Array", "Float64Array"]),

    /**
     * @param {*} array
     * @return {function(new:ArrayBufferView, ArrayBufferView)|null}
     */
    typedArrayClass: function(array)
    {
        var classes = TypeUtils._typedArrayClasses;
        for (var i = 0, n = classes.length; i < n; ++i) {
            if (array instanceof classes[i])
                return classes[i];
        }
        return null;
    },

    /**
     * @param {*} obj
     * @return {*}
     */
    clone: function(obj)
    {
        if (!obj)
            return obj;

        var type = typeof obj;
        if (type !== "object" && type !== "function")
            return obj;

        // Handle Array and ArrayBuffer instances.
        if (typeof obj.slice === "function") {
            console.assert(obj instanceof Array || obj instanceof ArrayBuffer);
            return obj.slice(0);
        }

        var typedArrayClass = TypeUtils.typedArrayClass(obj);
        if (typedArrayClass)
            return new typedArrayClass(/** @type {ArrayBufferView} */ (obj));

        if (obj instanceof HTMLImageElement) {
            var img = /** @type {HTMLImageElement} */ (obj);
            // Special case for Images with Blob URIs: cloneNode will fail if the Blob URI has already been revoked.
            // FIXME: Maybe this is a bug in WebKit core?
            if (/^blob:/.test(img.src)) {
                var canvas = inspectedWindow.document.createElement("canvas");
                var context = /** @type {CanvasRenderingContext2D} */ (Resource.wrappedObject(canvas.getContext("2d")));
                canvas.width = img.width;
                canvas.height = img.height;
                context.drawImage(img, 0, 0);
                return canvas;
            }
            return img.cloneNode(true);
        }

        if (obj instanceof HTMLCanvasElement) {
            var result = obj.cloneNode(true);
            var context = /** @type {CanvasRenderingContext2D} */ (Resource.wrappedObject(result.getContext("2d")));
            context.drawImage(obj, 0, 0);
            return result;
        }

        if (obj instanceof HTMLVideoElement) {
            var result = obj.cloneNode(true);
            // FIXME: Copy HTMLVideoElement's current image into a 2d canvas.
            return result;
        }

        if (obj instanceof ImageData) {
            var context = TypeUtils._dummyCanvas2dContext();
            // FIXME: suppress type checks due to outdated builtin externs for createImageData.
            var result = (/** @type {?} */ (context)).createImageData(obj);
            for (var i = 0, n = obj.data.length; i < n; ++i)
              result.data[i] = obj.data[i];
            return result;
        }

        console.error("ASSERT_NOT_REACHED: failed to clone object: ", obj);
        return obj;
    },

    /**
     * @param {Object=} obj
     * @return {Object}
     */
    cloneObject: function(obj)
    {
        if (!obj)
            return null;
        var result = {};
        for (var key in obj)
            result[key] = obj[key];
        return result;
    },

    /**
     * @return {CanvasRenderingContext2D}
     */
    _dummyCanvas2dContext: function()
    {
        var context = TypeUtils._dummyCanvas2dContextInstance;
        if (!context) {
            var canvas = inspectedWindow.document.createElement("canvas");
            context = /** @type {CanvasRenderingContext2D} */ (Resource.wrappedObject(canvas.getContext("2d")));
            TypeUtils._dummyCanvas2dContextInstance = context;
        }
        return context;
    }
}

/**
 * @interface
 */
function StackTrace()
{
}

StackTrace.prototype = {
    /**
     * @param {number} index
     * @return {{sourceURL: string, lineNumber: number, columnNumber: number}}
     */
    callFrame: function(index)
    {
    }
}

/**
 * @param {number=} stackTraceLimit
 * @param {Function=} topMostFunctionToIgnore
 * @return {StackTrace}
 */
StackTrace.create = function(stackTraceLimit, topMostFunctionToIgnore)
{
    if (typeof Error.captureStackTrace === "function")
        return new StackTraceV8(stackTraceLimit, topMostFunctionToIgnore || arguments.callee);
    // FIXME: Support JSC, and maybe other browsers.
    return null;
}

/**
 * @constructor
 * @implements {StackTrace}
 * @param {number=} stackTraceLimit
 * @param {Function=} topMostFunctionToIgnore
 * @see http://code.google.com/p/v8/wiki/JavaScriptStackTraceApi
 */
function StackTraceV8(stackTraceLimit, topMostFunctionToIgnore)
{
    StackTrace.call(this);
    var oldStackTraceLimit = Error.stackTraceLimit;
    if (typeof stackTraceLimit === "number")
        Error.stackTraceLimit = stackTraceLimit;

    this._error = /** @type {{stack: Array}} */ ({});
    Error.captureStackTrace(this._error, topMostFunctionToIgnore || arguments.callee);

    Error.stackTraceLimit = oldStackTraceLimit;
}

StackTraceV8.prototype = {
    /**
     * @override
     * @param {number} index
     * @return {{sourceURL: string, lineNumber: number, columnNumber: number}}
     */
    callFrame: function(index)
    {
        if (!this._stackTrace)
            this._prepareStackTrace();
        return this._stackTrace[index];
    },

    _prepareStackTrace: function()
    {
        var oldPrepareStackTrace = Error.prepareStackTrace;
        /**
         * @param {Object} error
         * @param {Array.<CallSite>} structuredStackTrace
         * @return {Array.<{sourceURL: string, lineNumber: number, columnNumber: number}>}
         */
        Error.prepareStackTrace = function(error, structuredStackTrace)
        {
            return structuredStackTrace.map(function(callSite) {
                return {
                    sourceURL: callSite.getFileName(),
                    lineNumber: callSite.getLineNumber(),
                    columnNumber: callSite.getColumnNumber()
                };
            });
        }
        this._stackTrace = this._error.stack;
        Error.prepareStackTrace = oldPrepareStackTrace;
        delete this._error; // No longer needed, free memory.
    },

    __proto__: StackTrace.prototype
}

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
        /** @type {!Object.<number, Object>} */
        this._items = Object.create(null);
        /** @type {number} */
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
 * @param {Resource|*=} result
 * @param {StackTrace=} stackTrace
 */
function Call(thisObject, functionName, args, result, stackTrace)
{
    this._thisObject = thisObject;
    this._functionName = functionName;
    this._args = Array.prototype.slice.call(args, 0);
    this._result = result;
    this._stackTrace = stackTrace || null;

    if (!this._functionName)
        console.assert(this._args.length === 2 && typeof this._args[0] === "string");
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
     * @return {boolean}
     */
    isPropertySetter: function()
    {
        return !this._functionName;
    },
    
    /**
     * @return {!Array}
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
    },

    /**
     * @return {StackTrace}
     */
    stackTrace: function()
    {
        return this._stackTrace;
    },

    /**
     * @param {StackTrace} stackTrace
     */
    setStackTrace: function(stackTrace)
    {
        this._stackTrace = stackTrace;
    },

    /**
     * @param {*} result
     */
    setResult: function(result)
    {
        this._result = result;
    },

    /**
     * @param {string} name
     * @param {Object} attachment
     */
    setAttachment: function(name, attachment)
    {
        if (attachment) {
            this._attachments = this._attachments || {};
            this._attachments[name] = attachment;
        } else if (this._attachments)
            delete this._attachments[name];
    },

    /**
     * @param {string} name
     * @return {Object}
     */
    attachment: function(name)
    {
        return this._attachments && this._attachments[name];
    },

    freeze: function()
    {
        if (this._freezed)
            return;
        this._freezed = true;
        for (var i = 0, n = this._args.length; i < n; ++i) {
            // FIXME: freeze the Resources also!
            if (!Resource.forObject(this._args[i]))
                this._args[i] = TypeUtils.clone(this._args[i]);
        }
    },

    /**
     * @param {!Cache} cache
     * @return {!ReplayableCall}
     */
    toReplayable: function(cache)
    {
        this.freeze();
        var thisObject = /** @type {ReplayableResource} */ (Resource.toReplayable(this._thisObject, cache));
        var result = Resource.toReplayable(this._result, cache);
        var args = this._args.map(function(obj) {
            return Resource.toReplayable(obj, cache);
        });
        var attachments = TypeUtils.cloneObject(this._attachments);
        return new ReplayableCall(thisObject, this._functionName, args, result, this._stackTrace, attachments);
    },

    /**
     * @param {!ReplayableCall} replayableCall
     * @param {!Cache} cache
     * @return {!Call}
     */
    replay: function(replayableCall, cache)
    {
        var replayObject = ReplayableResource.replay(replayableCall.resource(), cache);
        var replayArgs = replayableCall.args().map(function(obj) {
            return ReplayableResource.replay(obj, cache);
        });
        var replayResult = undefined;

        if (replayableCall.isPropertySetter())
            replayObject[replayArgs[0]] = replayArgs[1];
        else {
            var replayFunction = replayObject[replayableCall.functionName()];
            console.assert(typeof replayFunction === "function", "Expected a function to replay");
            replayResult = replayFunction.apply(replayObject, replayArgs);
            if (replayableCall.result() instanceof ReplayableResource) {
                var resource = replayableCall.result().replay(cache);
                if (!resource.wrappedObject())
                    resource.setWrappedObject(replayResult);
            }
        }
    
        this._thisObject = replayObject;
        this._functionName = replayableCall.functionName();
        this._args = replayArgs;
        this._result = replayResult;
        this._stackTrace = replayableCall.stackTrace();
        this._freezed = true;
        var attachments = replayableCall.attachments();
        if (attachments)
            this._attachments = TypeUtils.cloneObject(attachments);
        return this;
    }
}

/**
 * @constructor
 * @param {ReplayableResource} thisObject
 * @param {string} functionName
 * @param {Array.<ReplayableResource|*>} args
 * @param {ReplayableResource|*} result
 * @param {StackTrace} stackTrace
 * @param {Object.<string, Object>} attachments
 */
function ReplayableCall(thisObject, functionName, args, result, stackTrace, attachments)
{
    this._thisObject = thisObject;
    this._functionName = functionName;
    this._args = args;
    this._result = result;
    this._stackTrace = stackTrace;
    if (attachments)
        this._attachments = attachments;
}

ReplayableCall.prototype = {
    /**
     * @return {ReplayableResource}
     */
    resource: function()
    {
        return this._thisObject;
    },

    /**
     * @return {string}
     */
    functionName: function()
    {
        return this._functionName;
    },

    /**
     * @return {boolean}
     */
    isPropertySetter: function()
    {
        return !this._functionName;
    },

    /**
     * @return {Array.<ReplayableResource|*>}
     */
    args: function()
    {
        return this._args;
    },

    /**
     * @return {ReplayableResource|*}
     */
    result: function()
    {
        return this._result;
    },

    /**
     * @return {StackTrace}
     */
    stackTrace: function()
    {
        return this._stackTrace;
    },

    /**
     * @return {Object.<string, Object>}
     */
    attachments: function()
    {
        return this._attachments;
    },

    /**
     * @param {string} name
     * @return {Object}
     */
    attachment: function(name)
    {
        return this._attachments && this._attachments[name];
    },

    /**
     * @param {Cache} cache
     * @return {!Call}
     */
    replay: function(cache)
    {
        var call = /** @type {!Call} */ (Object.create(Call.prototype));
        return call.replay(this, cache);
    }
}

/**
 * @constructor
 * @param {!Object} wrappedObject
 */
function Resource(wrappedObject)
{
    /** @type {number} */
    this._id = ++Resource._uniqueId;
    /** @type {ResourceTrackingManager} */
    this._resourceManager = null;
    /** @type {!Array.<Call>} */
    this._calls = [];
    /**
     * This is to prevent GC from collecting associated resources.
     * Otherwise, for example in WebGL, subsequent calls to gl.getParameter()
     * may return a recently created instance that is no longer bound to a
     * Resource object (thus, no history to replay it later).
     *
     * @type {!Object.<string, Resource>}
     */
    this._boundResources = Object.create(null);
    this.setWrappedObject(wrappedObject);
}

/**
 * @type {number}
 */
Resource._uniqueId = 0;

/**
 * @param {*} obj
 * @return {Resource}
 */
Resource.forObject = function(obj)
{
    if (!obj)
        return null;
    if (obj instanceof Resource)
        return obj;
    if (typeof obj === "object")
        return obj["__resourceObject"];
    return null;
}

/**
 * @param {Resource|*} obj
 * @return {*}
 */
Resource.wrappedObject = function(obj)
{
    var resource = Resource.forObject(obj);
    return resource ? resource.wrappedObject() : obj;
}

/**
 * @param {Resource|*} obj
 * @param {!Cache} cache
 * @return {ReplayableResource|*}
 */
Resource.toReplayable = function(obj, cache)
{
    var resource = Resource.forObject(obj);
    return resource ? resource.toReplayable(cache) : obj;
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
     * @param {!Object} value
     */
    setWrappedObject: function(value)
    {
        console.assert(value, "wrappedObject should not be NULL");
        console.assert(!(value instanceof Resource), "Binding a Resource object to another Resource object?");
        this._wrappedObject = value;
        this._bindObjectToResource(value);
    },

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
     * @return {!Array.<Call>}
     */
    calls: function()
    {
        return this._calls;
    },

    /**
     * @param {!Cache} cache
     * @return {!ReplayableResource}
     */
    toReplayable: function(cache)
    {
        var result = /** @type {ReplayableResource} */ (cache.get(this._id));
        if (result)
            return result;
        var data = {
            id: this._id
        };
        result = new ReplayableResource(this, data);
        cache.put(this._id, result); // Put into the cache early to avoid loops.
        data.calls = this._calls.map(function(call) {
            return call.toReplayable(cache);
        });
        this._populateReplayableData(data, cache);
        return result;
    },

    /**
     * @param {!Object} data
     * @param {!Cache} cache
     */
    _populateReplayableData: function(data, cache)
    {
        // Do nothing. Should be overridden by subclasses.
    },

    /**
     * @param {!Object} data
     * @param {!Cache} cache
     * @return {!Resource}
     */
    replay: function(data, cache)
    {
        var resource = /** @type {Resource} */ (cache.get(data.id));
        if (resource)
            return resource;
        this._id = data.id;
        this._resourceManager = null;
        this._calls = [];
        this._wrappedObject = null;
        cache.put(data.id, this); // Put into the cache early to avoid loops.
        this._doReplayCalls(data, cache);
        console.assert(this._wrappedObject, "Resource should be reconstructed!");
        return this;
    },

    /**
     * @param {!Object} data
     * @param {!Cache} cache
     */
    _doReplayCalls: function(data, cache)
    {
        for (var i = 0, n = data.calls.length; i < n; ++i)
            this._calls.push(data.calls[i].replay(cache));
    },

    /**
     * @param {!Call} call
     */
    pushCall: function(call)
    {
        call.freeze();
        this._calls.push(call);
    },

    /**
     * @param {!Object} object
     */
    _bindObjectToResource: function(object)
    {
        Object.defineProperty(object, "__resourceObject", {
            value: this,
            writable: false,
            enumerable: false,
            configurable: true
        });
    },

    /**
     * @param {string} key
     * @param {*} obj
     */
    _registerBoundResource: function(key, obj)
    {
        var resource = Resource.forObject(obj);
        if (resource)
            this._boundResources[key] = resource;
        else
            delete this._boundResources[key];
    },

    /**
     * @return {Object}
     */
    _wrapObject: function()
    {
        var wrappedObject = this.wrappedObject();
        if (!wrappedObject)
            return null;
        var proxy = Object.create(wrappedObject.__proto__); // In order to emulate "instanceof".

        var self = this;
        var customWrapFunctions = this._customWrapFunctions();
        function processProperty(property)
        {
            if (typeof wrappedObject[property] === "function") {
                var customWrapFunction = customWrapFunctions[property];
                if (customWrapFunction)
                    proxy[property] = self._wrapCustomFunction(self, wrappedObject, wrappedObject[property], property, customWrapFunction);
                else
                    proxy[property] = self._wrapFunction(self, wrappedObject, wrappedObject[property], property);
            } else if (/^[A-Z0-9_]+$/.test(property) && typeof wrappedObject[property] === "number") {
                // Fast access to enums and constants.
                proxy[property] = wrappedObject[property];
            } else {
                Object.defineProperty(proxy, property, {
                    get: function()
                    {
                        var obj = wrappedObject[property];
                        var resource = Resource.forObject(obj);
                        return resource ? resource : obj;
                    },
                    set: self._wrapPropertySetter(self, wrappedObject, property),
                    enumerable: true
                });
            }
        }

        var isEmpty = true;
        for (var property in wrappedObject) {
            isEmpty = false;
            processProperty(property);
        }
        if (isEmpty)
            return wrappedObject; // Nothing to proxy.

        this._bindObjectToResource(proxy);
        return proxy;
    },

    /**
     * @param {!Resource} resource
     * @param {!Object} originalObject
     * @param {!Function} originalFunction
     * @param {string} functionName
     * @param {!Function} customWrapFunction
     * @return {!Function}
     */
    _wrapCustomFunction: function(resource, originalObject, originalFunction, functionName, customWrapFunction)
    {
        return function()
        {
            var manager = resource.manager();
            var isCapturing = manager && manager.capturing();
            if (isCapturing)
                manager.captureArguments(resource, arguments);
            var wrapFunction = new Resource.WrapFunction(originalObject, originalFunction, functionName, arguments);
            customWrapFunction.apply(wrapFunction, arguments);
            if (isCapturing) {
                var call = wrapFunction.call();
                call.setStackTrace(StackTrace.create(1, arguments.callee));
                manager.captureCall(call);
            }
            return wrapFunction.result();
        };
    },

    /**
     * @param {!Resource} resource
     * @param {!Object} originalObject
     * @param {!Function} originalFunction
     * @param {string} functionName
     * @return {!Function}
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
            var stackTrace = StackTrace.create(1, arguments.callee);
            var call = new Call(resource, functionName, arguments, result, stackTrace);
            manager.captureCall(call);
            return result;
        };
    },

    /**
     * @param {!Resource} resource
     * @param {!Object} originalObject
     * @param {string} propertyName
     * @return {function(*)}
     */
    _wrapPropertySetter: function(resource, originalObject, propertyName)
    {
        return function(value)
        {
            resource._registerBoundResource(propertyName, value);
            var manager = resource.manager();
            if (!manager || !manager.capturing()) {
                originalObject[propertyName] = Resource.wrappedObject(value);
                return;
            }
            var args = [propertyName, value];
            manager.captureArguments(resource, args);
            originalObject[propertyName] = Resource.wrappedObject(value);
            var stackTrace = StackTrace.create(1, arguments.callee);
            var call = new Call(resource, "", args, undefined, stackTrace);
            manager.captureCall(call);
        };
    },

    /**
     * @return {!Object.<string, Function>}
     */
    _customWrapFunctions: function()
    {
        return Object.create(null); // May be overridden by subclasses.
    }
}

/**
 * @constructor
 * @param {Object} originalObject
 * @param {Function} originalFunction
 * @param {string} functionName
 * @param {Array|Arguments} args
 */
Resource.WrapFunction = function(originalObject, originalFunction, functionName, args)
{
    this._originalObject = originalObject;
    this._originalFunction = originalFunction;
    this._functionName = functionName;
    this._args = args;
    this._resource = Resource.forObject(originalObject);
    console.assert(this._resource, "Expected a wrapped call on a Resource object.");
}

Resource.WrapFunction.prototype = {
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
     * @return {!Call}
     */
    call: function()
    {
        if (!this._call)
            this._call = new Call(this._resource, this._functionName, this._args, this.result());
        return this._call;
    },

    /**
     * @param {*} result
     */
    overrideResult: function(result)
    {
        var call = this.call();
        call.setResult(result);
        this._result = result;
    }
}

/**
 * @param {function(new:Resource, !Object)} resourceConstructor
 * @return {function(this:Resource.WrapFunction)}
 */
Resource.WrapFunction.resourceFactoryMethod = function(resourceConstructor)
{
    /** @this Resource.WrapFunction */
    return function()
    {
        var wrappedObject = /** @type {Object} */ (this.result());
        if (!wrappedObject)
            return;
        var resource = new resourceConstructor(wrappedObject);
        var manager = this._resource.manager();
        if (manager)
            manager.registerResource(resource);
        this.overrideResult(resource.proxyObject());
        resource.pushCall(this.call());
    }
}

/**
 * @constructor
 * @param {!Resource} originalResource
 * @param {!Object} data
 */
function ReplayableResource(originalResource, data)
{
    this._proto = originalResource.__proto__;
    this._data = data;
}

ReplayableResource.prototype = {
    /**
     * @param {Cache} cache
     * @return {!Resource}
     */
    replay: function(cache)
    {
        var result = /** @type {!Resource} */ (Object.create(this._proto));
        result = result.replay(this._data, cache)
        console.assert(result.__proto__ === this._proto, "Wrong type of a replay result");
        return result;
    }
}

/**
 * @param {ReplayableResource|*} obj
 * @param {Cache} cache
 * @return {*}
 */
ReplayableResource.replay = function(obj, cache)
{
    return (obj instanceof ReplayableResource) ? obj.replay(cache).wrappedObject() : obj;
}

/**
 * @constructor
 * @extends {Resource}
 */
function LogEverythingResource(wrappedObject)
{
    Resource.call(this, wrappedObject);
}

LogEverythingResource.prototype = {
    /**
     * @override
     * @return {!Object.<string, Function>}
     */
    _customWrapFunctions: function()
    {
        var wrapFunctions = Object.create(null);
        var wrappedObject = this.wrappedObject();
        if (wrappedObject) {
            for (var property in wrappedObject) {
                /** @this Resource.WrapFunction */
                wrapFunctions[property] = function()
                {
                    this._resource.pushCall(this.call());
                }
            }
        }
        return wrapFunctions;
    },

    __proto__: Resource.prototype
}

////////////////////////////////////////////////////////////////////////////////
// WebGL
////////////////////////////////////////////////////////////////////////////////

/**
 * @constructor
 * @extends {Resource}
 */
function WebGLBoundResource(wrappedObject)
{
    Resource.call(this, wrappedObject);
    /** @type {!Object.<string, *>} */
    this._state = {};
}

WebGLBoundResource.prototype = {
    /**
     * @override
     * @param {!Object} data
     * @param {!Cache} cache
     */
    _populateReplayableData: function(data, cache)
    {
        var state = this._state;
        data.state = {};
        Object.keys(state).forEach(function(parameter) {
            data.state[parameter] = Resource.toReplayable(state[parameter], cache);
        });
    },

    /**
     * @override
     * @param {!Object} data
     * @param {!Cache} cache
     */
    _doReplayCalls: function(data, cache)
    {
        var state = {};
        Object.keys(data.state).forEach(function(parameter) {
            state[parameter] = ReplayableResource.replay(data.state[parameter], cache);
        });
        this._state = state;

        var gl = this._replayContextResource(data, cache).wrappedObject();

        var bindingsData = {
            TEXTURE_2D: ["bindTexture", "TEXTURE_BINDING_2D"],
            TEXTURE_CUBE_MAP: ["bindTexture", "TEXTURE_BINDING_CUBE_MAP"],
            ARRAY_BUFFER: ["bindBuffer", "ARRAY_BUFFER_BINDING"],
            ELEMENT_ARRAY_BUFFER: ["bindBuffer", "ELEMENT_ARRAY_BUFFER_BINDING"],
            FRAMEBUFFER: ["bindFramebuffer", "FRAMEBUFFER_BINDING"],
            RENDERBUFFER: ["bindRenderbuffer", "RENDERBUFFER_BINDING"]
        };
        var originalBindings = {};
        Object.keys(bindingsData).forEach(function(bindingTarget) {
            var bindingParameter = bindingsData[bindingTarget][1];
            originalBindings[bindingTarget] = gl.getParameter(gl[bindingParameter]);
        });

        Resource.prototype._doReplayCalls.call(this, data, cache);

        Object.keys(bindingsData).forEach(function(bindingTarget) {
            var bindMethodName = bindingsData[bindingTarget][0];
            gl[bindMethodName].call(gl, gl[bindingTarget], originalBindings[bindingTarget]);
        });
    },

    /**
     * @param {!Object} data
     * @param {!Cache} cache
     * @return {WebGLRenderingContextResource}
     */
    _replayContextResource: function(data, cache)
    {
        var calls = data.calls;
        for (var i = 0, n = calls.length; i < n; ++i) {
            var resource = ReplayableResource.replay(calls[i].resource(), cache);
            var contextResource = WebGLRenderingContextResource.forObject(resource);
            if (contextResource)
                return contextResource;
        }
        return null;
    },

    /**
     * @param {number} target
     * @param {string} bindMethodName
     */
    pushBinding: function(target, bindMethodName)
    {
        if (this._state.BINDING !== target) {
            this._state.BINDING = target;
            this.pushCall(new Call(WebGLRenderingContextResource.forObject(this), bindMethodName, [target, this]));
        }
    },

    __proto__: Resource.prototype
}

/**
 * @constructor
 * @extends {WebGLBoundResource}
 */
function WebGLTextureResource(wrappedObject)
{
    WebGLBoundResource.call(this, wrappedObject);
}

WebGLTextureResource.prototype = {
    /**
     * @override
     * @param {!Object} data
     * @param {!Cache} cache
     */
    _doReplayCalls: function(data, cache)
    {
        var gl = this._replayContextResource(data, cache).wrappedObject();

        var state = {};
        WebGLRenderingContextResource.PixelStoreParameters.forEach(function(parameter) {
            state[parameter] = gl.getParameter(gl[parameter]);
        });

        WebGLBoundResource.prototype._doReplayCalls.call(this, data, cache);

        WebGLRenderingContextResource.PixelStoreParameters.forEach(function(parameter) {
            gl.pixelStorei(gl[parameter], state[parameter]);
        });
    },

    /**
     * @override
     * @param {!Call} call
     */
    pushCall: function(call)
    {
        var gl = WebGLRenderingContextResource.forObject(call.resource()).wrappedObject();
        WebGLRenderingContextResource.PixelStoreParameters.forEach(function(parameter) {
            var value = gl.getParameter(gl[parameter]);
            if (this._state[parameter] !== value) {
                this._state[parameter] = value;
                var pixelStoreCall = new Call(gl, "pixelStorei", [gl[parameter], value]);
                WebGLBoundResource.prototype.pushCall.call(this, pixelStoreCall);
            }
        }, this);

        // FIXME: remove any older calls that no longer contribute to the resource state.
        // FIXME: optimize memory usage: maybe it's more efficient to store one texImage2D call instead of many texSubImage2D.
        WebGLBoundResource.prototype.pushCall.call(this, call);
    },

    /**
     * Handles: texParameteri, texParameterf
     * @param {!Call} call
     */
    pushCall_texParameter: function(call)
    {
        var args = call.args();
        var pname = args[1];
        var param = args[2];
        if (this._state[pname] !== param) {
            this._state[pname] = param;
            WebGLBoundResource.prototype.pushCall.call(this, call);
        }
    },

    /**
     * Handles: copyTexImage2D, copyTexSubImage2D
     * copyTexImage2D and copyTexSubImage2D define a texture image with pixels from the current framebuffer.
     * @param {!Call} call
     */
    pushCall_copyTexImage2D: function(call)
    {
        var glResource = WebGLRenderingContextResource.forObject(call.resource());
        var gl = glResource.wrappedObject();
        var framebufferResource = /** @type {WebGLFramebufferResource} */ glResource.currentBinding(gl.FRAMEBUFFER);
        if (framebufferResource)
            this.pushCall(new Call(glResource, "bindFramebuffer", [gl.FRAMEBUFFER, framebufferResource]));
        else {
            // FIXME: Implement this case.
            console.error("ASSERT_NOT_REACHED: Could not properly process a gl." + call.functionName() + " call while the DRAWING BUFFER is bound.");
        }
        this.pushCall(call);
    },

    __proto__: WebGLBoundResource.prototype
}

/**
 * @constructor
 * @extends {Resource}
 */
function WebGLProgramResource(wrappedObject)
{
    Resource.call(this, wrappedObject);
}

WebGLProgramResource.prototype = {
    /**
     * @override (overrides @return type)
     * @return {WebGLProgram}
     */
    wrappedObject: function()
    {
        return this._wrappedObject;
    },

    /**
     * @override
     * @param {!Object} data
     * @param {!Cache} cache
     */
    _populateReplayableData: function(data, cache)
    {
        var glResource = WebGLRenderingContextResource.forObject(this);
        var gl = glResource.wrappedObject();
        var program = this.wrappedObject();

        var originalErrors = glResource.getAllErrors();

        var uniforms = [];
        var uniformsCount = /** @type {number} */ gl.getProgramParameter(program, gl.ACTIVE_UNIFORMS);
        for (var i = 0; i < uniformsCount; ++i) {
            var activeInfo = gl.getActiveUniform(program, i);
            if (!activeInfo)
                continue;
            var uniformLocation = gl.getUniformLocation(program, activeInfo.name);
            if (!uniformLocation)
                continue;
            var value = gl.getUniform(program, uniformLocation);
            uniforms.push({
                name: activeInfo.name,
                type: activeInfo.type,
                value: value
            });
        }
        data.uniforms = uniforms;

        glResource.restoreErrors(originalErrors);
    },

    /**
     * @override
     * @param {!Object} data
     * @param {!Cache} cache
     */
    _doReplayCalls: function(data, cache)
    {
        Resource.prototype._doReplayCalls.call(this, data, cache);
        var gl = WebGLRenderingContextResource.forObject(this).wrappedObject();
        var program = this.wrappedObject();

        var originalProgram = /** @type {WebGLProgram} */ (gl.getParameter(gl.CURRENT_PROGRAM));
        var currentProgram = originalProgram;
        
        data.uniforms.forEach(function(uniform) {
            var uniformLocation = gl.getUniformLocation(program, uniform.name);
            if (!uniformLocation)
                return;
            if (currentProgram !== program) {
                currentProgram = program;
                gl.useProgram(program);
            }
            var methodName = this._uniformMethodNameByType(gl, uniform.type);
            if (methodName.indexOf("Matrix") === -1)
                gl[methodName].call(gl, uniformLocation, uniform.value);
            else
                gl[methodName].call(gl, uniformLocation, false, uniform.value);
        }.bind(this));

        if (currentProgram !== originalProgram)
            gl.useProgram(originalProgram);
    },

    /**
     * @param {WebGLRenderingContext} gl
     * @param {number} type
     * @return {string}
     */
    _uniformMethodNameByType: function(gl, type)
    {
        var uniformMethodNames = WebGLProgramResource._uniformMethodNames;
        if (!uniformMethodNames) {
            uniformMethodNames = {};
            uniformMethodNames[gl.FLOAT] = "uniform1f";
            uniformMethodNames[gl.FLOAT_VEC2] = "uniform2fv";
            uniformMethodNames[gl.FLOAT_VEC3] = "uniform3fv";
            uniformMethodNames[gl.FLOAT_VEC4] = "uniform4fv";
            uniformMethodNames[gl.INT] = "uniform1i";
            uniformMethodNames[gl.BOOL] = "uniform1i";
            uniformMethodNames[gl.SAMPLER_2D] = "uniform1i";
            uniformMethodNames[gl.SAMPLER_CUBE] = "uniform1i";
            uniformMethodNames[gl.INT_VEC2] = "uniform2iv";
            uniformMethodNames[gl.BOOL_VEC2] = "uniform2iv";
            uniformMethodNames[gl.INT_VEC3] = "uniform3iv";
            uniformMethodNames[gl.BOOL_VEC3] = "uniform3iv";
            uniformMethodNames[gl.INT_VEC4] = "uniform4iv";
            uniformMethodNames[gl.BOOL_VEC4] = "uniform4iv";
            uniformMethodNames[gl.FLOAT_MAT2] = "uniformMatrix2fv";
            uniformMethodNames[gl.FLOAT_MAT3] = "uniformMatrix3fv";
            uniformMethodNames[gl.FLOAT_MAT4] = "uniformMatrix4fv";
            WebGLProgramResource._uniformMethodNames = uniformMethodNames;
        }
        console.assert(uniformMethodNames[type], "Unknown uniform type " + type);
        return uniformMethodNames[type];
    },

    /**
     * @override
     * @param {!Call} call
     */
    pushCall: function(call)
    {
        // FIXME: remove any older calls that no longer contribute to the resource state.
        // FIXME: handle multiple attachShader && detachShader.
        Resource.prototype.pushCall.call(this, call);
    },

    __proto__: Resource.prototype
}

/**
 * @constructor
 * @extends {Resource}
 */
function WebGLShaderResource(wrappedObject)
{
    Resource.call(this, wrappedObject);
}

WebGLShaderResource.prototype = {
    /**
     * @return {number}
     */
    type: function()
    {
        var call = this._calls[0];
        if (call && call.functionName() === "createShader")
            return call.args()[0];
        console.error("ASSERT_NOT_REACHED: Failed to restore shader type from the log.", call);
        return 0;
    },

    /**
     * @override
     * @param {!Call} call
     */
    pushCall: function(call)
    {
        // FIXME: remove any older calls that no longer contribute to the resource state.
        // FIXME: handle multiple shaderSource calls.
        Resource.prototype.pushCall.call(this, call);
    },

    __proto__: Resource.prototype
}

/**
 * @constructor
 * @extends {WebGLBoundResource}
 */
function WebGLBufferResource(wrappedObject)
{
    WebGLBoundResource.call(this, wrappedObject);
}

WebGLBufferResource.prototype = {
    /**
     * @override
     * @param {!Call} call
     */
    pushCall: function(call)
    {
        // FIXME: remove any older calls that no longer contribute to the resource state.
        // FIXME: Optimize memory for bufferSubData.
        WebGLBoundResource.prototype.pushCall.call(this, call);
    },

    __proto__: WebGLBoundResource.prototype
}

/**
 * @constructor
 * @extends {WebGLBoundResource}
 */
function WebGLFramebufferResource(wrappedObject)
{
    WebGLBoundResource.call(this, wrappedObject);
}

WebGLFramebufferResource.prototype = {
    /**
     * @override
     * @param {!Call} call
     */
    pushCall: function(call)
    {
        // FIXME: remove any older calls that no longer contribute to the resource state.
        WebGLBoundResource.prototype.pushCall.call(this, call);
    },

    __proto__: WebGLBoundResource.prototype
}

/**
 * @constructor
 * @extends {WebGLBoundResource}
 */
function WebGLRenderbufferResource(wrappedObject)
{
    WebGLBoundResource.call(this, wrappedObject);
}

WebGLRenderbufferResource.prototype = {
    /**
     * @override
     * @param {!Call} call
     */
    pushCall: function(call)
    {
        // FIXME: remove any older calls that no longer contribute to the resource state.
        WebGLBoundResource.prototype.pushCall.call(this, call);
    },

    __proto__: WebGLBoundResource.prototype
}

/**
 * @constructor
 * @extends {Resource}
 * @param {!WebGLRenderingContext} glContext
 * @param {function():WebGLRenderingContext} replayContextCallback
 */
function WebGLRenderingContextResource(glContext, replayContextCallback)
{
    Resource.call(this, glContext);
    this._replayContextCallback = replayContextCallback;
    /** @type {Object.<number, boolean>} */
    this._customErrors = null;
    /** @type {!Object.<string, boolean>} */
    this._extensions = {};
}

/**
 * @const
 * @type {!Array.<string>}
 */
WebGLRenderingContextResource.GLCapabilities = [
    "BLEND",
    "CULL_FACE",
    "DEPTH_TEST",
    "DITHER",
    "POLYGON_OFFSET_FILL",
    "SAMPLE_ALPHA_TO_COVERAGE",
    "SAMPLE_COVERAGE",
    "SCISSOR_TEST",
    "STENCIL_TEST"
];

/**
 * @const
 * @type {!Array.<string>}
 */
WebGLRenderingContextResource.PixelStoreParameters = [
    "PACK_ALIGNMENT",
    "UNPACK_ALIGNMENT",
    "UNPACK_COLORSPACE_CONVERSION_WEBGL",
    "UNPACK_FLIP_Y_WEBGL",
    "UNPACK_PREMULTIPLY_ALPHA_WEBGL"
];

/**
 * @const
 * @type {!Array.<string>}
 */
WebGLRenderingContextResource.StateParameters = [
    "ACTIVE_TEXTURE",
    "ARRAY_BUFFER_BINDING",
    "BLEND_COLOR",
    "BLEND_DST_ALPHA",
    "BLEND_DST_RGB",
    "BLEND_EQUATION_ALPHA",
    "BLEND_EQUATION_RGB",
    "BLEND_SRC_ALPHA",
    "BLEND_SRC_RGB",
    "COLOR_CLEAR_VALUE",
    "COLOR_WRITEMASK",
    "CULL_FACE_MODE",
    "CURRENT_PROGRAM",
    "DEPTH_CLEAR_VALUE",
    "DEPTH_FUNC",
    "DEPTH_RANGE",
    "DEPTH_WRITEMASK",
    "ELEMENT_ARRAY_BUFFER_BINDING",
    "FRAMEBUFFER_BINDING",
    "FRONT_FACE",
    "GENERATE_MIPMAP_HINT",
    "LINE_WIDTH",
    "PACK_ALIGNMENT",
    "POLYGON_OFFSET_FACTOR",
    "POLYGON_OFFSET_UNITS",
    "RENDERBUFFER_BINDING",
    "SAMPLE_COVERAGE_INVERT",
    "SAMPLE_COVERAGE_VALUE",
    "SCISSOR_BOX",
    "STENCIL_BACK_FAIL",
    "STENCIL_BACK_FUNC",
    "STENCIL_BACK_PASS_DEPTH_FAIL",
    "STENCIL_BACK_PASS_DEPTH_PASS",
    "STENCIL_BACK_REF",
    "STENCIL_BACK_VALUE_MASK",
    "STENCIL_BACK_WRITEMASK",
    "STENCIL_CLEAR_VALUE",
    "STENCIL_FAIL",
    "STENCIL_FUNC",
    "STENCIL_PASS_DEPTH_FAIL",
    "STENCIL_PASS_DEPTH_PASS",
    "STENCIL_REF",
    "STENCIL_VALUE_MASK",
    "STENCIL_WRITEMASK",
    "UNPACK_ALIGNMENT",
    "UNPACK_COLORSPACE_CONVERSION_WEBGL",
    "UNPACK_FLIP_Y_WEBGL",
    "UNPACK_PREMULTIPLY_ALPHA_WEBGL",
    "VIEWPORT"
];

/**
 * @param {*} obj
 * @return {WebGLRenderingContextResource}
 */
WebGLRenderingContextResource.forObject = function(obj)
{
    var resource = Resource.forObject(obj);
    if (!resource || resource instanceof WebGLRenderingContextResource)
        return resource;
    var calls = resource.calls();
    if (!calls || !calls.length)
        return null;
    resource = calls[0].resource();
    return (resource instanceof WebGLRenderingContextResource) ? resource : null;
}

WebGLRenderingContextResource.prototype = {
    /**
     * @override (overrides @return type)
     * @return {WebGLRenderingContext}
     */
    wrappedObject: function()
    {
        return this._wrappedObject;
    },

    /**
     * @return {Array.<number>}
     */
    getAllErrors: function()
    {
        var errors = [];
        var gl = this.wrappedObject();
        if (gl) {
            while (true) {
                var error = gl.getError();
                if (error === gl.NO_ERROR)
                    break;
                this.clearError(error);
                errors.push(error);
            }
        }
        if (this._customErrors) {
            for (var key in this._customErrors) {
                var error = Number(key);
                errors.push(error);
            }
            delete this._customErrors;
        }
        return errors;
    },

    /**
     * @param {Array.<number>} errors
     */
    restoreErrors: function(errors)
    {
        var gl = this.wrappedObject();
        if (gl) {
            var wasError = false;
            while (gl.getError() !== gl.NO_ERROR)
                wasError = true;
            console.assert(!wasError, "Error(s) while capturing current WebGL state.");
        }
        if (!errors.length)
            delete this._customErrors;
        else {
            this._customErrors = {};
            for (var i = 0, n = errors.length; i < n; ++i)
                this._customErrors[errors[i]] = true;
        }
    },

    /**
     * @param {number} error
     */
    clearError: function(error)
    {
        if (this._customErrors)
            delete this._customErrors[error];
    },

    /**
     * @return {number}
     */
    nextError: function()
    {
        if (this._customErrors) {
            for (var key in this._customErrors) {
                var error = Number(key);
                delete this._customErrors[error];
                return error;
            }
        }
        delete this._customErrors;
        var gl = this.wrappedObject();
        return gl ? gl.NO_ERROR : 0;
    },

    /**
     * @param {string} name
     */
    addExtension: function(name)
    {
        // FIXME: Wrap OES_vertex_array_object extension.
        this._extensions[name] = true;
    },

    /**
     * @override
     * @param {!Object} data
     * @param {!Cache} cache
     */
    _populateReplayableData: function(data, cache)
    {
        var gl = this.wrappedObject();
        data.replayContextCallback = this._replayContextCallback;
        data.extensions = TypeUtils.cloneObject(this._extensions);

        var originalErrors = this.getAllErrors();

        // Take a full GL state snapshot.
        var glState = {};
        WebGLRenderingContextResource.GLCapabilities.forEach(function(parameter) {
            glState[parameter] = gl.isEnabled(gl[parameter]);
        });
        WebGLRenderingContextResource.StateParameters.forEach(function(parameter) {
            glState[parameter] = Resource.toReplayable(gl.getParameter(gl[parameter]), cache);
        });

        // VERTEX_ATTRIB_ARRAYS
        var maxVertexAttribs = /** @type {number} */ (gl.getParameter(gl.MAX_VERTEX_ATTRIBS));
        var vertexAttribParameters = ["VERTEX_ATTRIB_ARRAY_BUFFER_BINDING", "VERTEX_ATTRIB_ARRAY_ENABLED", "VERTEX_ATTRIB_ARRAY_SIZE", "VERTEX_ATTRIB_ARRAY_STRIDE", "VERTEX_ATTRIB_ARRAY_TYPE", "VERTEX_ATTRIB_ARRAY_NORMALIZED", "CURRENT_VERTEX_ATTRIB"];
        var vertexAttribStates = [];
        for (var i = 0; i < maxVertexAttribs; ++i) {
            var state = {};
            vertexAttribParameters.forEach(function(attribParameter) {
                state[attribParameter] = Resource.toReplayable(gl.getVertexAttrib(i, gl[attribParameter]), cache);
            });
            state.VERTEX_ATTRIB_ARRAY_POINTER = gl.getVertexAttribOffset(i, gl.VERTEX_ATTRIB_ARRAY_POINTER);
            vertexAttribStates.push(state);
        }
        glState.vertexAttribStates = vertexAttribStates;

        // TEXTURES
        var currentTextureBinding = /** @type {number} */ (gl.getParameter(gl.ACTIVE_TEXTURE));
        var maxTextureImageUnits = /** @type {number} */ (gl.getParameter(gl.MAX_TEXTURE_IMAGE_UNITS));
        var textureBindings = [];
        for (var i = 0; i < maxTextureImageUnits; ++i) {
            gl.activeTexture(gl.TEXTURE0 + i);
            var state = {
                TEXTURE_2D: Resource.toReplayable(gl.getParameter(gl.TEXTURE_BINDING_2D), cache),
                TEXTURE_CUBE_MAP: Resource.toReplayable(gl.getParameter(gl.TEXTURE_BINDING_CUBE_MAP), cache)
            };
            textureBindings.push(state);
        }
        glState.textureBindings = textureBindings;
        gl.activeTexture(currentTextureBinding);

        data.glState = glState;

        this.restoreErrors(originalErrors);
    },

    /**
     * @override
     * @param {!Object} data
     * @param {!Cache} cache
     */
    _doReplayCalls: function(data, cache)
    {
        this._replayContextCallback = data.replayContextCallback;
        this._customErrors = null;
        this._extensions = TypeUtils.cloneObject(data.extensions) || {};

        var gl = /** @type {!WebGLRenderingContext} */ (Resource.wrappedObject(this._replayContextCallback()));
        this.setWrappedObject(gl);

        // Enable corresponding WebGL extensions.
        for (var name in this._extensions)
            gl.getExtension(name);

        var glState = data.glState;
        gl.bindFramebuffer(gl.FRAMEBUFFER, /** @type {WebGLFramebuffer} */ (ReplayableResource.replay(glState.FRAMEBUFFER_BINDING, cache)));
        gl.bindRenderbuffer(gl.RENDERBUFFER, /** @type {WebGLRenderbuffer} */ (ReplayableResource.replay(glState.RENDERBUFFER_BINDING, cache)));

        // Enable or disable server-side GL capabilities.
        WebGLRenderingContextResource.GLCapabilities.forEach(function(parameter) {
            console.assert(parameter in glState);
            if (glState[parameter])
                gl.enable(gl[parameter]);
            else
                gl.disable(gl[parameter]);
        });

        gl.blendColor(glState.BLEND_COLOR[0], glState.BLEND_COLOR[1], glState.BLEND_COLOR[2], glState.BLEND_COLOR[3]);
        gl.blendEquationSeparate(glState.BLEND_EQUATION_RGB, glState.BLEND_EQUATION_ALPHA);
        gl.blendFuncSeparate(glState.BLEND_SRC_RGB, glState.BLEND_DST_RGB, glState.BLEND_SRC_ALPHA, glState.BLEND_DST_ALPHA);
        gl.clearColor(glState.COLOR_CLEAR_VALUE[0], glState.COLOR_CLEAR_VALUE[1], glState.COLOR_CLEAR_VALUE[2], glState.COLOR_CLEAR_VALUE[3]);
        gl.clearDepth(glState.DEPTH_CLEAR_VALUE);
        gl.clearStencil(glState.STENCIL_CLEAR_VALUE);
        gl.colorMask(glState.COLOR_WRITEMASK[0], glState.COLOR_WRITEMASK[1], glState.COLOR_WRITEMASK[2], glState.COLOR_WRITEMASK[3]);
        gl.cullFace(glState.CULL_FACE_MODE);
        gl.depthFunc(glState.DEPTH_FUNC);
        gl.depthMask(glState.DEPTH_WRITEMASK);
        gl.depthRange(glState.DEPTH_RANGE[0], glState.DEPTH_RANGE[1]);
        gl.frontFace(glState.FRONT_FACE);
        gl.hint(gl.GENERATE_MIPMAP_HINT, glState.GENERATE_MIPMAP_HINT);
        gl.lineWidth(glState.LINE_WIDTH);

        WebGLRenderingContextResource.PixelStoreParameters.forEach(function(parameter) {
            gl.pixelStorei(gl[parameter], glState[parameter]);
        });

        gl.polygonOffset(glState.POLYGON_OFFSET_FACTOR, glState.POLYGON_OFFSET_UNITS);
        gl.sampleCoverage(glState.SAMPLE_COVERAGE_VALUE, glState.SAMPLE_COVERAGE_INVERT);
        gl.stencilFuncSeparate(gl.FRONT, glState.STENCIL_FUNC, glState.STENCIL_REF, glState.STENCIL_VALUE_MASK);
        gl.stencilFuncSeparate(gl.BACK, glState.STENCIL_BACK_FUNC, glState.STENCIL_BACK_REF, glState.STENCIL_BACK_VALUE_MASK);
        gl.stencilOpSeparate(gl.FRONT, glState.STENCIL_FAIL, glState.STENCIL_PASS_DEPTH_FAIL, glState.STENCIL_PASS_DEPTH_PASS);
        gl.stencilOpSeparate(gl.BACK, glState.STENCIL_BACK_FAIL, glState.STENCIL_BACK_PASS_DEPTH_FAIL, glState.STENCIL_BACK_PASS_DEPTH_PASS);
        gl.stencilMaskSeparate(gl.FRONT, glState.STENCIL_WRITEMASK);
        gl.stencilMaskSeparate(gl.BACK, glState.STENCIL_BACK_WRITEMASK);

        gl.scissor(glState.SCISSOR_BOX[0], glState.SCISSOR_BOX[1], glState.SCISSOR_BOX[2], glState.SCISSOR_BOX[3]);
        gl.viewport(glState.VIEWPORT[0], glState.VIEWPORT[1], glState.VIEWPORT[2], glState.VIEWPORT[3]);

        gl.useProgram(/** @type {WebGLProgram} */ (ReplayableResource.replay(glState.CURRENT_PROGRAM, cache)));

        // VERTEX_ATTRIB_ARRAYS
        var maxVertexAttribs = /** @type {number} */ (gl.getParameter(gl.MAX_VERTEX_ATTRIBS));
        for (var i = 0; i < maxVertexAttribs; ++i) {
            var state = glState.vertexAttribStates[i] || {};
            if (state.VERTEX_ATTRIB_ARRAY_ENABLED)
                gl.enableVertexAttribArray(i);
            else
                gl.disableVertexAttribArray(i);
            if (state.CURRENT_VERTEX_ATTRIB)
                gl.vertexAttrib4fv(i, state.CURRENT_VERTEX_ATTRIB);
            var buffer = /** @type {WebGLBuffer} */ (ReplayableResource.replay(state.VERTEX_ATTRIB_ARRAY_BUFFER_BINDING, cache));
            if (buffer) {
                gl.bindBuffer(gl.ARRAY_BUFFER, buffer);
                gl.vertexAttribPointer(i, state.VERTEX_ATTRIB_ARRAY_SIZE, state.VERTEX_ATTRIB_ARRAY_TYPE, state.VERTEX_ATTRIB_ARRAY_NORMALIZED, state.VERTEX_ATTRIB_ARRAY_STRIDE, state.VERTEX_ATTRIB_ARRAY_POINTER);
            }
        }
        gl.bindBuffer(gl.ARRAY_BUFFER, /** @type {WebGLBuffer} */ (ReplayableResource.replay(glState.ARRAY_BUFFER_BINDING, cache)));
        gl.bindBuffer(gl.ELEMENT_ARRAY_BUFFER, /** @type {WebGLBuffer} */ (ReplayableResource.replay(glState.ELEMENT_ARRAY_BUFFER_BINDING, cache)));

        // TEXTURES
        var maxTextureImageUnits = /** @type {number} */ (gl.getParameter(gl.MAX_TEXTURE_IMAGE_UNITS));
        for (var i = 0; i < maxTextureImageUnits; ++i) {
            gl.activeTexture(gl.TEXTURE0 + i);
            var state = glState.textureBindings[i] || {};
            gl.bindTexture(gl.TEXTURE_2D, /** @type {WebGLTexture} */ (ReplayableResource.replay(state.TEXTURE_2D, cache)));
            gl.bindTexture(gl.TEXTURE_CUBE_MAP, /** @type {WebGLTexture} */ (ReplayableResource.replay(state.TEXTURE_CUBE_MAP, cache)));
        }
        gl.activeTexture(glState.ACTIVE_TEXTURE);

        Resource.prototype._doReplayCalls.call(this, data, cache);
    },

    /**
     * @param {Object|number} target
     * @return {Resource}
     */
    currentBinding: function(target)
    {
        var resource = Resource.forObject(target);
        if (resource)
            return resource;
        var gl = this.wrappedObject();
        var bindingParameter;
        var bindMethodName;
        var bindMethodTarget = target;
        switch (target) {
        case gl.ARRAY_BUFFER:
            bindingParameter = gl.ARRAY_BUFFER_BINDING;
            bindMethodName = "bindBuffer";
            break;
        case gl.ELEMENT_ARRAY_BUFFER:
            bindingParameter = gl.ELEMENT_ARRAY_BUFFER_BINDING;
            bindMethodName = "bindBuffer";
            break;
        case gl.TEXTURE_2D:
            bindingParameter = gl.TEXTURE_BINDING_2D;
            bindMethodName = "bindTexture";
            break;
        case gl.TEXTURE_CUBE_MAP:
        case gl.TEXTURE_CUBE_MAP_POSITIVE_X:
        case gl.TEXTURE_CUBE_MAP_NEGATIVE_X:
        case gl.TEXTURE_CUBE_MAP_POSITIVE_Y:
        case gl.TEXTURE_CUBE_MAP_NEGATIVE_Y:
        case gl.TEXTURE_CUBE_MAP_POSITIVE_Z:
        case gl.TEXTURE_CUBE_MAP_NEGATIVE_Z:
            bindingParameter = gl.TEXTURE_BINDING_CUBE_MAP;
            bindMethodTarget = gl.TEXTURE_CUBE_MAP;
            bindMethodName = "bindTexture";
            break;
        case gl.FRAMEBUFFER:
            bindingParameter = gl.FRAMEBUFFER_BINDING;
            bindMethodName = "bindFramebuffer";
            break;
        case gl.RENDERBUFFER:
            bindingParameter = gl.RENDERBUFFER_BINDING;
            bindMethodName = "bindRenderbuffer";
            break;
        default:
            console.error("ASSERT_NOT_REACHED: unknown binding target " + target);
            return null;
        }
        resource = Resource.forObject(gl.getParameter(bindingParameter));
        if (resource)
            resource.pushBinding(bindMethodTarget, bindMethodName);
        return resource;
    },

    /**
     * @override
     * @return {!Object.<string, Function>}
     */
    _customWrapFunctions: function()
    {
        var wrapFunctions = WebGLRenderingContextResource._wrapFunctions;
        if (!wrapFunctions) {
            wrapFunctions = Object.create(null);

            wrapFunctions["createBuffer"] = Resource.WrapFunction.resourceFactoryMethod(WebGLBufferResource);
            wrapFunctions["createShader"] = Resource.WrapFunction.resourceFactoryMethod(WebGLShaderResource);
            wrapFunctions["createProgram"] = Resource.WrapFunction.resourceFactoryMethod(WebGLProgramResource);
            wrapFunctions["createTexture"] = Resource.WrapFunction.resourceFactoryMethod(WebGLTextureResource);
            wrapFunctions["createFramebuffer"] = Resource.WrapFunction.resourceFactoryMethod(WebGLFramebufferResource);
            wrapFunctions["createRenderbuffer"] = Resource.WrapFunction.resourceFactoryMethod(WebGLRenderbufferResource);
            wrapFunctions["getUniformLocation"] = Resource.WrapFunction.resourceFactoryMethod(Resource);

            /**
             * @param {string} methodName
             * @param {function(this:Resource, !Call)=} pushCallFunc
             */
            function stateModifyingWrapFunction(methodName, pushCallFunc)
            {
                if (pushCallFunc) {
                    /**
                     * @param {Object|number} target
                     * @this Resource.WrapFunction
                     */
                    wrapFunctions[methodName] = function(target)
                    {
                        var resource = this._resource.currentBinding(target);
                        if (resource)
                            pushCallFunc.call(resource, this.call());
                    }
                } else {
                    /**
                     * @param {Object|number} target
                     * @this Resource.WrapFunction
                     */
                    wrapFunctions[methodName] = function(target)
                    {
                        var resource = this._resource.currentBinding(target);
                        if (resource)
                            resource.pushCall(this.call());
                    }
                }
            }
            stateModifyingWrapFunction("bindAttribLocation");
            stateModifyingWrapFunction("compileShader");
            stateModifyingWrapFunction("detachShader");
            stateModifyingWrapFunction("linkProgram");
            stateModifyingWrapFunction("shaderSource");
            stateModifyingWrapFunction("bufferData");
            stateModifyingWrapFunction("bufferSubData");
            stateModifyingWrapFunction("compressedTexImage2D");
            stateModifyingWrapFunction("compressedTexSubImage2D");
            stateModifyingWrapFunction("copyTexImage2D", WebGLTextureResource.prototype.pushCall_copyTexImage2D);
            stateModifyingWrapFunction("copyTexSubImage2D", WebGLTextureResource.prototype.pushCall_copyTexImage2D);
            stateModifyingWrapFunction("generateMipmap");
            stateModifyingWrapFunction("texImage2D");
            stateModifyingWrapFunction("texSubImage2D");
            stateModifyingWrapFunction("texParameterf", WebGLTextureResource.prototype.pushCall_texParameter);
            stateModifyingWrapFunction("texParameteri", WebGLTextureResource.prototype.pushCall_texParameter);
            stateModifyingWrapFunction("renderbufferStorage");

            /** @this Resource.WrapFunction */
            wrapFunctions["getError"] = function()
            {
                var gl = /** @type {WebGLRenderingContext} */ (this._originalObject);
                var error = this.result();
                if (error !== gl.NO_ERROR)
                    this._resource.clearError(error);
                else {
                    error = this._resource.nextError();
                    if (error !== gl.NO_ERROR)
                        this.overrideResult(error);
                }
            }

            /**
             * @param {string} name
             * @this Resource.WrapFunction
             */
            wrapFunctions["getExtension"] = function(name)
            {
                this._resource.addExtension(name);
            }

            //
            // Register bound WebGL resources.
            //

            /**
             * @param {WebGLProgram} program
             * @param {WebGLShader} shader
             * @this Resource.WrapFunction
             */
            wrapFunctions["attachShader"] = function(program, shader)
            {
                var resource = this._resource.currentBinding(program);
                if (resource) {
                    resource.pushCall(this.call());
                    var shaderResource = /** @type {WebGLShaderResource} */ (Resource.forObject(shader));
                    if (shaderResource) {
                        var shaderType = shaderResource.type();
                        resource._registerBoundResource("__attachShader_" + shaderType, shaderResource);
                    }
                }
            }
            /**
             * @param {number} target
             * @param {number} attachment
             * @param {number} objectTarget
             * @param {WebGLRenderbuffer|WebGLTexture} obj
             * @this Resource.WrapFunction
             */
            wrapFunctions["framebufferRenderbuffer"] = wrapFunctions["framebufferTexture2D"] = function(target, attachment, objectTarget, obj)
            {
                var resource = this._resource.currentBinding(target);
                if (resource) {
                    resource.pushCall(this.call());
                    resource._registerBoundResource("__framebufferAttachmentObjectName", obj);
                }
            }
            /**
             * @param {number} target
             * @param {Object} obj
             * @this Resource.WrapFunction
             */
            wrapFunctions["bindBuffer"] = wrapFunctions["bindFramebuffer"] = wrapFunctions["bindRenderbuffer"] = function(target, obj)
            {
                this._resource._registerBoundResource("__bindBuffer_" + target, obj);
            }
            /**
             * @param {number} target
             * @param {WebGLTexture} obj
             * @this Resource.WrapFunction
             */
            wrapFunctions["bindTexture"] = function(target, obj)
            {
                var gl = /** @type {WebGLRenderingContext} */ (this._originalObject);
                var currentTextureBinding = /** @type {number} */ (gl.getParameter(gl.ACTIVE_TEXTURE));
                this._resource._registerBoundResource("__bindTexture_" + target + "_" + currentTextureBinding, obj);
            }
            /**
             * @param {WebGLProgram} program
             * @this Resource.WrapFunction
             */
            wrapFunctions["useProgram"] = function(program)
            {
                this._resource._registerBoundResource("__useProgram", program);
            }
            /**
             * @param {number} index
             * @this Resource.WrapFunction
             */
            wrapFunctions["vertexAttribPointer"] = function(index)
            {
                var gl = /** @type {WebGLRenderingContext} */ (this._originalObject);
                this._resource._registerBoundResource("__vertexAttribPointer_" + index, gl.getParameter(gl.ARRAY_BUFFER_BINDING));
            }

            WebGLRenderingContextResource._wrapFunctions = wrapFunctions;
        }
        return wrapFunctions;
    },

    __proto__: Resource.prototype
}

////////////////////////////////////////////////////////////////////////////////
// 2D Canvas
////////////////////////////////////////////////////////////////////////////////

/**
 * @constructor
 * @extends {Resource}
 * @param {!CanvasRenderingContext2D} context
 * @param {function():CanvasRenderingContext2D} replayContextCallback
 */
function CanvasRenderingContext2DResource(context, replayContextCallback)
{
    Resource.call(this, context);
    this._replayContextCallback = replayContextCallback;
}

/**
 * @const
 * @type {!Array.<string>}
 */
CanvasRenderingContext2DResource.AttributeProperties = [
    "strokeStyle",
    "fillStyle",
    "globalAlpha",
    "lineWidth",
    "lineCap",
    "lineJoin",
    "miterLimit",
    "shadowOffsetX",
    "shadowOffsetY",
    "shadowBlur",
    "shadowColor",
    "globalCompositeOperation",
    "font",
    "textAlign",
    "textBaseline",
    "lineDashOffset",
    // FIXME: Temporary properties implemented in JSC, but not in V8.
    "webkitLineDash",
    "webkitLineDashOffset"
];

/**
 * @const
 * @type {!Array.<string>}
 */
CanvasRenderingContext2DResource.PathMethods = [
    "beginPath",
    "moveTo",
    "closePath",
    "lineTo",
    "quadraticCurveTo",
    "bezierCurveTo",
    "arcTo",
    "arc",
    "rect"
];

/**
 * @const
 * @type {!Array.<string>}
 */
CanvasRenderingContext2DResource.TransformationMatrixMethods = [
    "scale",
    "rotate",
    "translate",
    "transform",
    "setTransform"
];

CanvasRenderingContext2DResource.prototype = {
    /**
     * @override (overrides @return type)
     * @return {CanvasRenderingContext2D}
     */
    wrappedObject: function()
    {
        return this._wrappedObject;
    },

    /**
     * @override
     * @param {!Object} data
     * @param {!Cache} cache
     */
    _populateReplayableData: function(data, cache)
    {
        data.replayContextCallback = this._replayContextCallback;
        data.currentAttributes = this._currentAttributesState();
        var ctx = this.wrappedObject();
        try {
            data.originalImageData = ctx.getImageData(0, 0, ctx.canvas.width, ctx.canvas.height);
        } catch (e) {
            console.error("ASSERT_NOT_REACHED: getImageData failed.", e);
        }
    },

    /**
     * @override
     * @param {!Object} data
     * @param {!Cache} cache
     */
    _doReplayCalls: function(data, cache)
    {
        this._replayContextCallback = data.replayContextCallback;

        var ctx = /** @type {!CanvasRenderingContext2D} */ (Resource.wrappedObject(this._replayContextCallback()));
        this.setWrappedObject(ctx);

        if (data.originalImageData) {
            try {
                ctx.putImageData(data.originalImageData, 0, 0);
            } catch (e) {
                console.error("ASSERT_NOT_REACHED: putImageData failed.", e);
            }
        }

        for (var i = 0, n = data.calls.length; i < n; ++i) {
            var replayableCall = data.calls[i];
            if (replayableCall.functionName() === "save")
                this._applyAttributesState(replayableCall.attachment("canvas2dAttributesState"));
            this._calls.push(replayableCall.replay(cache));
        }
        this._applyAttributesState(data.currentAttributes);
    },

    /**
     * @param {!Call} call
     */
    pushCall_setTransform: function(call)
    {
        var saveCallIndex = this._lastIndexOfMatchingSaveCall();
        var index = this._lastIndexOfAnyCall(CanvasRenderingContext2DResource.PathMethods);
        index = Math.max(index, saveCallIndex);
        if (this._removeCallsFromLog(CanvasRenderingContext2DResource.TransformationMatrixMethods, index + 1))
            this._removeAllObsoleteCallsFromLog();
        this.pushCall(call);
    },

    /**
     * @param {!Call} call
     */
    pushCall_beginPath: function(call)
    {
        var index = this._lastIndexOfAnyCall(["clip"]);
        if (this._removeCallsFromLog(CanvasRenderingContext2DResource.PathMethods, index + 1))
            this._removeAllObsoleteCallsFromLog();
        this.pushCall(call);
    },

    /**
     * @param {!Call} call
     */
    pushCall_save: function(call)
    {
        call.setAttachment("canvas2dAttributesState", this._currentAttributesState());
        this.pushCall(call);
    },

    /**
     * @param {!Call} call
     */
    pushCall_restore: function(call)
    {
        var lastIndexOfSave = this._lastIndexOfMatchingSaveCall();
        if (lastIndexOfSave === -1)
            return;
        this._calls[lastIndexOfSave].setAttachment("canvas2dAttributesState", null); // No longer needed, free memory.

        var modified = false;
        if (this._removeCallsFromLog(["clip"], lastIndexOfSave + 1))
            modified = true;

        var lastIndexOfAnyPathMethod = this._lastIndexOfAnyCall(CanvasRenderingContext2DResource.PathMethods);
        var index = Math.max(lastIndexOfSave, lastIndexOfAnyPathMethod);
        if (this._removeCallsFromLog(CanvasRenderingContext2DResource.TransformationMatrixMethods, index + 1))
            modified = true;

        if (modified)
            this._removeAllObsoleteCallsFromLog();

        var lastCall = this._calls[this._calls.length - 1];
        if (lastCall && lastCall.functionName() === "save")
            this._calls.pop();
        else
            this.pushCall(call);
    },

    /**
     * @param {number=} fromIndex
     * @return {number}
     */
    _lastIndexOfMatchingSaveCall: function(fromIndex)
    {
        if (typeof fromIndex !== "number")
            fromIndex = this._calls.length - 1;
        else
            fromIndex = Math.min(fromIndex, this._calls.length - 1);
        var stackDepth = 1;
        for (var i = fromIndex; i >= 0; --i) {
            var functionName = this._calls[i].functionName();
            if (functionName === "restore")
                ++stackDepth;
            else if (functionName === "save") {
                --stackDepth;
                if (!stackDepth)
                    return i;
            }
        }
        return -1;
    },

    /**
     * @param {!Array.<string>} functionNames
     * @param {number=} fromIndex
     * @return {number}
     */
    _lastIndexOfAnyCall: function(functionNames, fromIndex)
    {
        if (typeof fromIndex !== "number")
            fromIndex = this._calls.length - 1;
        else
            fromIndex = Math.min(fromIndex, this._calls.length - 1);
        for (var i = fromIndex; i >= 0; --i) {
            if (functionNames.indexOf(this._calls[i].functionName()) !== -1)
                return i;
        }
        return -1;
    },

    _removeAllObsoleteCallsFromLog: function()
    {
        // Remove all PATH methods between clip() and beginPath() calls.
        var lastIndexOfBeginPath = this._lastIndexOfAnyCall(["beginPath"]);
        while (lastIndexOfBeginPath !== -1) {
            var index = this._lastIndexOfAnyCall(["clip"], lastIndexOfBeginPath - 1);
            this._removeCallsFromLog(CanvasRenderingContext2DResource.PathMethods, index + 1, lastIndexOfBeginPath);
            lastIndexOfBeginPath = this._lastIndexOfAnyCall(["beginPath"], index - 1);
        }

        // Remove all TRASFORMATION MATRIX methods before restore() or setTransform() but after any PATH or corresponding save() method.
        var lastRestore = this._lastIndexOfAnyCall(["restore", "setTransform"]);
        while (lastRestore !== -1) {
            var saveCallIndex = this._lastIndexOfMatchingSaveCall(lastRestore - 1);
            var index = this._lastIndexOfAnyCall(CanvasRenderingContext2DResource.PathMethods, lastRestore - 1);
            index = Math.max(index, saveCallIndex);
            this._removeCallsFromLog(CanvasRenderingContext2DResource.TransformationMatrixMethods, index + 1, lastRestore);
            lastRestore = this._lastIndexOfAnyCall(["restore", "setTransform"], index - 1);
        }

        // Remove all save-restore consecutive pairs.
        var restoreCalls = 0;
        for (var i = this._calls.length - 1; i >= 0; --i) {
            var functionName = this._calls[i].functionName();
            if (functionName === "restore") {
                ++restoreCalls;
                continue;
            }
            if (functionName === "save" && restoreCalls > 0) {
                var saveCallIndex = i;
                for (var j = i - 1; j >= 0 && i - j < restoreCalls; --j) {
                    if (this._calls[j].functionName() === "save")
                        saveCallIndex = j;
                    else
                        break;
                }
                this._calls.splice(saveCallIndex, (i - saveCallIndex + 1) * 2);
                i = saveCallIndex;
            }
            restoreCalls = 0;
        }
    },

    /**
     * @param {!Array.<string>} functionNames
     * @param {number} fromIndex
     * @param {number=} toIndex
     * @return {boolean}
     */
    _removeCallsFromLog: function(functionNames, fromIndex, toIndex)
    {
        var oldLength = this._calls.length;
        if (typeof toIndex !== "number")
            toIndex = oldLength;
        else
            toIndex = Math.min(toIndex, oldLength);
        var newIndex = Math.min(fromIndex, oldLength);
        for (var i = newIndex; i < toIndex; ++i) {
            var call = this._calls[i];
            if (functionNames.indexOf(call.functionName()) === -1)
                this._calls[newIndex++] = call;
        }
        if (newIndex >= toIndex)
            return false;
        this._calls.splice(newIndex, toIndex - newIndex);
        return true;
    },

    /**
     * @return {!Object.<string, string>}
     */
    _currentAttributesState: function()
    {
        var ctx = this.wrappedObject();
        var state = {};
        state.attributes = {};
        CanvasRenderingContext2DResource.AttributeProperties.forEach(function(attribute) {
            state.attributes[attribute] = ctx[attribute];
        });
        if (ctx.getLineDash)
            state.lineDash = ctx.getLineDash();
        return state;
    },

    /**
     * @param {Object.<string, string>=} state
     */
    _applyAttributesState: function(state)
    {
        if (!state)
            return;
        var ctx = this.wrappedObject();
        if (state.attributes) {
            Object.keys(state.attributes).forEach(function(attribute) {
                ctx[attribute] = state.attributes[attribute];
            });
        }
        if (ctx.setLineDash)
            ctx.setLineDash(state.lineDash);
    },

    /**
     * @override
     * @return {!Object.<string, Function>}
     */
    _customWrapFunctions: function()
    {
        var wrapFunctions = CanvasRenderingContext2DResource._wrapFunctions;
        if (!wrapFunctions) {
            wrapFunctions = Object.create(null);

            wrapFunctions["createLinearGradient"] = Resource.WrapFunction.resourceFactoryMethod(LogEverythingResource);
            wrapFunctions["createRadialGradient"] = Resource.WrapFunction.resourceFactoryMethod(LogEverythingResource);
            wrapFunctions["createPattern"] = Resource.WrapFunction.resourceFactoryMethod(LogEverythingResource);

            /**
             * @param {string} methodName
             * @param {function(this:Resource, !Call)=} func
             */
            function stateModifyingWrapFunction(methodName, func)
            {
                if (func) {
                    /** @this Resource.WrapFunction */
                    wrapFunctions[methodName] = function()
                    {
                        func.call(this._resource, this.call());
                    }
                } else {
                    /** @this Resource.WrapFunction */
                    wrapFunctions[methodName] = function()
                    {
                        this._resource.pushCall(this.call());
                    }
                }
            }

            for (var i = 0, methodName; methodName = CanvasRenderingContext2DResource.TransformationMatrixMethods[i]; ++i)
                stateModifyingWrapFunction(methodName, methodName === "setTransform" ? this.pushCall_setTransform : undefined);
            for (var i = 0, methodName; methodName = CanvasRenderingContext2DResource.PathMethods[i]; ++i)
                stateModifyingWrapFunction(methodName, methodName === "beginPath" ? this.pushCall_beginPath : undefined);

            stateModifyingWrapFunction("save", this.pushCall_save);
            stateModifyingWrapFunction("restore", this.pushCall_restore);
            stateModifyingWrapFunction("clip");

            CanvasRenderingContext2DResource._wrapFunctions = wrapFunctions;
        }
        return wrapFunctions;
    },

    __proto__: Resource.prototype
}

/**
 * @constructor
 */
function TraceLog()
{
    /** @type {!Array.<ReplayableCall>} */
    this._replayableCalls = [];
    /** @type {!Cache} */
    this._replayablesCache = new Cache();
}

TraceLog.prototype = {
    /**
     * @return {number}
     */
    size: function()
    {
        return this._replayableCalls.length;
    },

    /**
     * @return {!Array.<ReplayableCall>}
     */
    replayableCalls: function()
    {
        return this._replayableCalls;
    },

    /**
     * @param {!Resource} resource
     */
    captureResource: function(resource)
    {
        resource.toReplayable(this._replayablesCache);
    },

    /**
     * @param {!Call} call
     */
    addCall: function(call)
    {
        var res = Resource.forObject(call.result());
        if (res)
            this.captureResource(res);
        var size = this._replayablesCache.size();
        this._replayableCalls.push(call.toReplayable(this._replayablesCache));
        console.assert(this._replayablesCache.size() === size, "Internal error: We should have captured all the resources already by this time.");
    }
}

/**
 * @constructor
 * @param {!TraceLog} traceLog
 * @param {function()=} resetCallback
 */
function TraceLogPlayer(traceLog, resetCallback)
{
    /** @type {!TraceLog} */
    this._traceLog = traceLog;
    /** @type {number} */
    this._nextReplayStep = 0;
    /** @type {!Cache} */
    this._replayWorldCache = new Cache();
    /** @type {function()|undefined} */
    this._resetCallback = resetCallback;
}

TraceLogPlayer.prototype = {
    /**
     * @return {!TraceLog}
     */
    traceLog: function()
    {
        return this._traceLog;
    },

    /**
     * @return {number}
     */
    nextReplayStep: function()
    {
        return this._nextReplayStep;
    },

    reset: function()
    {
        this._nextReplayStep = 0;
        this._replayWorldCache.reset();
        if (this._resetCallback)
            this._resetCallback();
    },

    /**
     * @return {Call}
     */
    step: function()
    {
        return this.stepTo(this._nextReplayStep);
    },

    /**
     * @param {number} stepNum
     * @return {Call}
     */
    stepTo: function(stepNum)
    {
        stepNum = Math.min(stepNum, this._traceLog.size() - 1);
        console.assert(stepNum >= 0);
        if (this._nextReplayStep > stepNum)
            this.reset();
        // FIXME: Replay all the cached resources first to warm-up.
        var lastCall = null;
        var replayableCalls = this._traceLog.replayableCalls();
        while (this._nextReplayStep <= stepNum)
            lastCall = replayableCalls[this._nextReplayStep++].replay(this._replayWorldCache);
        return lastCall;
    },

    /**
     * @return {Call}
     */
    replay: function()
    {
        return this.stepTo(this._traceLog.size() - 1);
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
     * @param {!Resource} resource
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

    /**
     * @param {!Resource} resource
     * @param {Array|Arguments} args
     */
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
     * @param {!Call} call
     */
    captureCall: function(call)
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
     * @param {function()} callback
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
    /** @type {!ResourceTrackingManager} */
    this._manager = new ResourceTrackingManager();
    /** @type {number} */
    this._lastTraceLogId = 0;
    /** @type {!Object.<string, TraceLog>} */
    this._traceLogs = {};
    /** @type {TraceLogPlayer} */
    this._traceLogPlayer = null;
    /** @type {!Array.<{type: string, context: Object}>} */
    this._replayContexts = [];
}

InjectedScript.prototype = {
    /**
     * @param {!WebGLRenderingContext} glContext
     * @return {Object}
     */
    wrapWebGLContext: function(glContext)
    {
        var resource = Resource.forObject(glContext) || new WebGLRenderingContextResource(glContext, this._constructWebGLReplayContext.bind(this, glContext));
        this._manager.registerResource(resource);
        return resource.proxyObject();
    },

    /**
     * @param {!CanvasRenderingContext2D} context
     * @return {Object}
     */
    wrapCanvas2DContext: function(context)
    {
        var resource = Resource.forObject(context) || new CanvasRenderingContext2DResource(context, this._constructCanvas2DReplayContext.bind(this, context));
        this._manager.registerResource(resource);
        return resource.proxyObject();
    },

    /**
     * @return {string}
     */
    captureFrame: function()
    {
        return this._callStartCapturingFunction(this._manager.captureFrame);
    },

    /**
     * @return {string}
     */
    startCapturing: function()
    {
        return this._callStartCapturingFunction(this._manager.startCapturing);
    },

    /**
     * @param {function(this:ResourceTrackingManager)} func
     * @return {string}
     */
    _callStartCapturingFunction: function(func)
    {
        var oldTraceLog = this._manager.lastTraceLog();
        func.call(this._manager);
        var traceLog = this._manager.lastTraceLog();
        if (traceLog === oldTraceLog) {
            for (var id in this._traceLogs) {
                if (this._traceLogs[id] === traceLog)
                    return id;
            }
        }
        var id = this._makeTraceLogId();
        this._traceLogs[id] = traceLog;
        return id;
    },

    /**
     * @param {string} id
     */
    stopCapturing: function(id)
    {
        var traceLog = this._traceLogs[id];
        if (traceLog)
            this._manager.stopCapturing(traceLog);
    },

    /**
     * @param {string} id
     */
    dropTraceLog: function(id)
    {
        this.stopCapturing(id);
        if (this._traceLogPlayer && this._traceLogPlayer.traceLog() === this._traceLogs[id]) {
            this._traceLogPlayer = null;
            this._replayContexts = [];
        }
        delete this._traceLogs[id];
    },

    /**
     * @param {string} id
     * @param {number=} startOffset
     * @return {Object|string}
     */
    traceLog: function(id, startOffset)
    {
        var traceLog = this._traceLogs[id];
        if (!traceLog)
            return "Error: Trace log with this ID not found.";
        startOffset = Math.max(0, startOffset || 0);
        var alive = this._manager.capturing() && this._manager.lastTraceLog() === traceLog;
        var result = {
            id: id,
            calls: [],
            alive: alive,
            startOffset: startOffset
        };
        var calls = traceLog.replayableCalls();
        for (var i = startOffset, n = calls.length; i < n; ++i) {
            var call = calls[i];
            var args = call.args().map(function(argument) {
                return argument + "";
            });
            var stackTrace = call.stackTrace();
            var callFrame = stackTrace ? stackTrace.callFrame(0) || {} : {};
            var traceLogItem = {
                sourceURL: callFrame.sourceURL,
                lineNumber: callFrame.lineNumber,
                columnNumber: callFrame.columnNumber
            };
            if (call.functionName()) {
                traceLogItem.functionName = call.functionName();
                traceLogItem.arguments = args;
            } else {
                traceLogItem.property = args[0];
                traceLogItem.value = args[1];
            }
            var callResult = call.result();
            if (callResult !== undefined && callResult !== null)
                traceLogItem.result = callResult + "";
            result.calls.push(traceLogItem);
        }
        return result;
    },

    /**
     * @param {string} id
     * @param {number} stepNo
     * @return {string}
     */
    replayTraceLog: function(id, stepNo)
    {
        var traceLog = this._traceLogs[id];
        if (!traceLog)
            return "";
        if (!this._traceLogPlayer || this._traceLogPlayer.traceLog() !== traceLog) {
            this._replayContexts = [];
            this._traceLogPlayer = new TraceLogPlayer(traceLog, this._onTraceLogPlayerReset.bind(this));
        }
        var lastCall = this._traceLogPlayer.stepTo(stepNo);
        if (!this._replayContexts.length) {
            console.error("ASSERT_NOT_REACHED: replayTraceLog failed to create a replay canvas?!");
            return "";
        }
        // FIXME: Support replaying several canvases simultaneously.
        var lastCallResourceContext = Resource.wrappedObject(lastCall.resource());
        for (var i = 0, n = this._replayContexts.length; i < n; ++i) {
            var context = this._replayContexts[i].context;
            if (lastCallResourceContext === context)
                return context.canvas.toDataURL();
        }
        console.assert("ASSERT_NOT_REACHED: replayTraceLog failed to match the replaying canvas?!");
        return this._replayContexts[0].context.canvas.toDataURL();
    },

    /**
     * @return {string}
     */
    _makeTraceLogId: function()
    {
        return "{\"injectedScriptId\":" + injectedScriptId + ",\"traceLogId\":" + (++this._lastTraceLogId) + "}";
    },

    _onTraceLogPlayerReset: function()
    {
        this._replayContexts = [];
    },

    /**
     * @param {!WebGLRenderingContext} originalGlContext
     * @return {WebGLRenderingContext}
     */
    _constructWebGLReplayContext: function(originalGlContext)
    {
        var canvas = originalGlContext.canvas.cloneNode(true);
        var attributes = originalGlContext.getContextAttributes();
        var contextIds = ["experimental-webgl", "webkit-3d", "3d"];
        for (var i = 0, contextId; contextId = contextIds[i]; ++i) {
            var replayContext = canvas.getContext(contextId, attributes);
            if (replayContext) {
                replayContext = /** @type {WebGLRenderingContext} */ (Resource.wrappedObject(replayContext));
                this._replayContexts.push({
                    type: "3d",
                    context: replayContext
                });
                return replayContext;
            }
        }
        return null;
    },

    /**
     * @param {!CanvasRenderingContext2D} originalContext
     * @return {CanvasRenderingContext2D}
     */
    _constructCanvas2DReplayContext: function(originalContext)
    {
        // Create a new 2D context each time to start with an empty context drawing state stack (managed by save() and restore() methods).
        var canvas = originalContext.canvas.cloneNode(true);
        var replayContext = /** @type {CanvasRenderingContext2D} */ (Resource.wrappedObject(canvas.getContext("2d")));
        this._replayContexts.push({
            type: "2d",
            context: replayContext
        });
        return replayContext;
    }
}

var injectedScript = new InjectedScript();
return injectedScript;

})
