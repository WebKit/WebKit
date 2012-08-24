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
     * @type {Array.<Function>}
     */
    typedArrayClasses: (function(typeNames) {
        var result = [];
        for (var i = 0, n = typeNames.length; i < n; ++i) {
            if (inspectedWindow[typeNames[i]])
                result.push(inspectedWindow[typeNames[i]]);
        }
        return result;
    })(["Int8Array", "Uint8Array", "Uint8ClampedArray", "Int16Array", "Uint16Array", "Int32Array", "Uint32Array", "Float32Array", "Float64Array"]),

    /**
     * @param {*} array
     * @return {Function}
     */
    typedArrayClass: function(array)
    {
        var classes = TypeUtils.typedArrayClasses;
        for (var i = 0, n = classes.length; i < n; ++i) {
            if (array instanceof classes[i])
                return classes[i];
        }
        return null;
    },

    /**
     * @param {*} obj
     * @return {*}
     * FIXME: suppress checkTypes due to outdated builtin externs for CanvasRenderingContext2D and ImageData
     * @suppress {checkTypes}
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
            return new typedArrayClass(obj);

        if (obj instanceof HTMLImageElement)
            return obj.cloneNode(true);

        if (obj instanceof HTMLCanvasElement) {
            var result = obj.cloneNode(true);
            var context = result.getContext("2d");
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
            var result = context.createImageData(obj);
            for (var i = 0, n = obj.data.length; i < n; ++i)
              result.data[i] = obj.data[i];
            return result;
        }

        console.error("ASSERT_NOT_REACHED: failed to clone object: ", obj);
        return obj;
    },

    /**
     * @return {CanvasRenderingContext2D}
     */
    _dummyCanvas2dContext: function()
    {
        var context = TypeUtils._dummyCanvas2dContext;
        if (!context) {
            var canvas = inspectedWindow.document.createElement("canvas");
            context = canvas.getContext("2d");
            var contextResource = Resource.forObject(context);
            if (contextResource)
                context = contextResource.wrappedObject();
            TypeUtils._dummyCanvas2dContext = context;
        }
        return context;
    }
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
 * @param {Resource|*=} result
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
    }
}

/**
 * @constructor
 * @param {ReplayableResource} thisObject
 * @param {string} functionName
 * @param {Array.<ReplayableResource|*>} args
 * @param {ReplayableResource|*} result
 */
function ReplayableCall(thisObject, functionName, args, result)
{
    this._thisObject = thisObject;
    this._functionName = functionName;
    this._args = args;
    this._result = result;
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
     * @param {Cache} cache
     * @return {Call}
     */
    replay: function(cache)
    {
        // FIXME: Do the replay.
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
    this._calls = [];
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
     * @return {Array.<Call>}
     */
    calls: function()
    {
        return this._calls;
    },

    /**
     * @param {Call} call
     */
    pushCall: function(call)
    {
        call.freeze();
        this._calls.push(call);
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
 * @param {Resource} originalResource
 * @param {Object} data
 */
function ReplayableResource(originalResource, data)
{
}

ReplayableResource.prototype = {
    /**
     * @param {Cache} cache
     * @return {Resource}
     */
    replay: function(cache)
    {
        // FIXME: Do the replay.
    }
}

/**
 * @constructor
 * @extends {Resource}
 */
function WebGLBoundResource(wrappedObject)
{
    Resource.call(this, wrappedObject);
    this._state = {};
}

WebGLBoundResource.prototype = {
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
    }
}

WebGLBoundResource.prototype.__proto__ = Resource.prototype;

/**
 * @constructor
 * @extends {WebGLBoundResource}
 */
function WebGLTextureResource(wrappedObject)
{
    WebGLBoundResource.call(this, wrappedObject);
}

WebGLTextureResource.prototype = {
    /** @inheritDoc */
    pushCall: function(call)
    {
        var gl = call.resource().wrappedObject();
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
     * @param {Call} call
     */
    pushCall_texParameterf: function(call)
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
     * copyTexImage2D and copyTexSubImage2D define a texture image with pixels from the current framebuffer.
     * @param {Call} call
     */
    pushCall_copyTexImage2D: function(call)
    {
        var glResource = call.resource();
        var gl = glResource.wrappedObject();
        var framebufferResource = glResource.currentBinding(gl.FRAMEBUFFER);
        if (framebufferResource)
            this.pushCall(new Call(glResource, "bindFramebuffer", [gl.FRAMEBUFFER, framebufferResource]));
        else
            console.error("ASSERT_NOT_REACHED: No FRAMEBUFFER bound while calling gl." + call.functionName());
        this.pushCall(call);
    }
}

WebGLTextureResource.prototype.pushCall_texParameteri = WebGLTextureResource.prototype.pushCall_texParameterf;
WebGLTextureResource.prototype.pushCall_copyTexSubImage2D = WebGLTextureResource.prototype.pushCall_copyTexImage2D;

WebGLTextureResource.prototype.__proto__ = WebGLBoundResource.prototype;

/**
 * @constructor
 * @extends {Resource}
 */
function WebGLProgramResource(wrappedObject)
{
    Resource.call(this, wrappedObject);
}

WebGLProgramResource.prototype = {
    /** @inheritDoc */
    pushCall: function(call)
    {
        // FIXME: remove any older calls that no longer contribute to the resource state.
        // FIXME: handle multiple attachShader && detachShader.
        Resource.prototype.pushCall.call(this, call);
    }
}

WebGLProgramResource.prototype.__proto__ = Resource.prototype;

/**
 * @constructor
 * @extends {Resource}
 */
function WebGLShaderResource(wrappedObject)
{
    Resource.call(this, wrappedObject);
}

WebGLShaderResource.prototype = {
    /** @inheritDoc */
    pushCall: function(call)
    {
        // FIXME: remove any older calls that no longer contribute to the resource state.
        // FIXME: handle multiple shaderSource calls.
        Resource.prototype.pushCall.call(this, call);
    }
}

WebGLShaderResource.prototype.__proto__ = Resource.prototype;

/**
 * @constructor
 * @extends {WebGLBoundResource}
 */
function WebGLBufferResource(wrappedObject)
{
    WebGLBoundResource.call(this, wrappedObject);
}

WebGLBufferResource.prototype = {
    /** @inheritDoc */
    pushCall: function(call)
    {
        // FIXME: remove any older calls that no longer contribute to the resource state.
        // FIXME: Optimize memory for bufferSubData.
        WebGLBoundResource.prototype.pushCall.call(this, call);
    }
}

WebGLBufferResource.prototype.__proto__ = WebGLBoundResource.prototype;

/**
 * @constructor
 * @extends {WebGLBoundResource}
 */
function WebGLFramebufferResource(wrappedObject)
{
    WebGLBoundResource.call(this, wrappedObject);
}

WebGLFramebufferResource.prototype = {
    /** @inheritDoc */
    pushCall: function(call)
    {
        // FIXME: remove any older calls that no longer contribute to the resource state.
        WebGLBoundResource.prototype.pushCall.call(this, call);
    }
}

WebGLFramebufferResource.prototype.__proto__ = WebGLBoundResource.prototype;

/**
 * @constructor
 * @extends {WebGLBoundResource}
 */
function WebGLRenderbufferResource(wrappedObject)
{
    WebGLBoundResource.call(this, wrappedObject);
}

WebGLRenderbufferResource.prototype = {
    /** @inheritDoc */
    pushCall: function(call)
    {
        // FIXME: remove any older calls that no longer contribute to the resource state.
        WebGLBoundResource.prototype.pushCall.call(this, call);
    }
}

WebGLRenderbufferResource.prototype.__proto__ = WebGLBoundResource.prototype;

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

/**
 * @const
 * @type {Array.<string>}
 */
WebGLRenderingContextResource.PixelStoreParameters = [
    "PACK_ALIGNMENT",
    "UNPACK_ALIGNMENT",
    "UNPACK_COLORSPACE_CONVERSION_WEBGL",
    "UNPACK_FLIP_Y_WEBGL",
    "UNPACK_PREMULTIPLY_ALPHA_WEBGL"
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
    var call = resource.calls();
    if (!call || !call.length)
        return null;
    resource = call[0].resource();
    return (resource instanceof WebGLRenderingContextResource) ? resource : null;
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
     * @param {Object|number} target
     * @return {Resource}
     */
    currentBinding: function(target)
    {
        var resource = Resource.forObject(target);
        if (resource)
            return resource;
        var gl = this.wrappedObject();
        var bindingTarget;
        var bindMethodName;
        switch (target) {
            case gl.ARRAY_BUFFER:
                bindingTarget = gl.ARRAY_BUFFER_BINDING;
                bindMethodName = "bindBuffer";
                break;
            case gl.ELEMENT_ARRAY_BUFFER:
                bindingTarget = gl.ELEMENT_ARRAY_BUFFER_BINDING;
                bindMethodName = "bindBuffer";
                break;
            case gl.TEXTURE_2D:
                bindingTarget = gl.TEXTURE_BINDING_2D;
                bindMethodName = "bindTexture";
                break;
            case gl.TEXTURE_CUBE_MAP:
            case gl.TEXTURE_CUBE_MAP_POSITIVE_X:
            case gl.TEXTURE_CUBE_MAP_NEGATIVE_X:
            case gl.TEXTURE_CUBE_MAP_POSITIVE_Y:
            case gl.TEXTURE_CUBE_MAP_NEGATIVE_Y:
            case gl.TEXTURE_CUBE_MAP_POSITIVE_Z:
            case gl.TEXTURE_CUBE_MAP_NEGATIVE_Z:
                bindingTarget = gl.TEXTURE_BINDING_CUBE_MAP;
                bindMethodName = "bindTexture";
                break;
            case gl.FRAMEBUFFER:
                bindingTarget = gl.FRAMEBUFFER_BINDING;
                bindMethodName = "bindFramebuffer";
                break;
            case gl.RENDERBUFFER:
                bindingTarget = gl.RENDERBUFFER_BINDING;
                bindMethodName = "bindRenderbuffer";
                break;
            default:
                console.error("ASSERT_NOT_REACHED: unknown binding target " + target);
                return null;
        }
        resource = Resource.forObject(gl.getParameter(bindingTarget));
        if (resource)
            resource.pushBinding(target, bindMethodName);
        return resource;
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
                var customWrapFunction = WebGLRenderingContextResource.WrapFunctions[property];
                if (customWrapFunction)
                    proxy[property] = self._wrapCustomFunction(self, glContext, glContext[property], property, customWrapFunction);
                else
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
     * @param {Function} customWrapFunction
     * @return {*}
     */
    _wrapCustomFunction: function(resource, originalObject, originalFunction, functionName, customWrapFunction)
    {
        return function()
        {
            var manager = resource.manager();
            if (manager)
                manager.captureArguments(resource, arguments);
            var wrapFunction = new WebGLRenderingContextResource.WrapFunction(originalObject, originalFunction, functionName, arguments);
            customWrapFunction.apply(wrapFunction, arguments);
            if (manager)
                manager.reportCall(wrapFunction.call());
            return wrapFunction.result();
        };
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
 * @param {Array|Arguments} args
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

WebGLRenderingContextResource.WrapFunctions = {
    __proto__: null
};

(function(items) {
    items.forEach(function(item) {
        var methodName = item[0];
        var resourceConstructor = item[1];
        WebGLRenderingContextResource.WrapFunctions[methodName] = function()
        {
            var wrappedObject = this.result();
            if (!wrappedObject)
                return;
            var resource = new resourceConstructor(wrappedObject);
            var manager = this._glResource.manager();
            if (manager)
                manager.registerResource(resource);
            resource.pushCall(this.call());
        };
    });
})([
    ["createBuffer", WebGLBufferResource],
    ["createShader", WebGLShaderResource],
    ["createProgram", WebGLProgramResource],
    ["createTexture", WebGLTextureResource],
    ["createFramebuffer", WebGLFramebufferResource],
    ["createRenderbuffer", WebGLRenderbufferResource],
    ["getUniformLocation", Resource]
]);

(function(methods) {
    methods.forEach(function(methodName) {
        var customPushCall = "pushCall_" + methodName;
        WebGLRenderingContextResource.WrapFunctions[methodName] = function(target)
        {
            var resource = this._glResource.currentBinding(target);
            if (resource) {
                if (resource[customPushCall])
                    resource[customPushCall].call(resource, this.call());
                else
                    resource.pushCall(this.call());
            }
        };
    });
})([
    "attachShader",
    "bindAttribLocation",
    "compileShader",
    "detachShader",
    "linkProgram",
    "shaderSource",
    "bufferData",
    "bufferSubData",
    "compressedTexImage2D",
    "compressedTexSubImage2D",
    "copyTexImage2D",
    "copyTexSubImage2D",
    "generateMipmap",
    "texImage2D",
    "texSubImage2D",
    "texParameterf",
    "texParameteri",
    "framebufferRenderbuffer",
    "framebufferTexture2D",
    "renderbufferStorage"
]);

/**
 * @constructor
 */
function TraceLog()
{
    this._replayableCalls = [];
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
     * @return {Array.<ReplayableCall>}
     */
    replayableCalls: function()
    {
        return this._replayableCalls;
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
        // FIXME: Convert the call to a ReplayableCall and push it.
    }
}

/**
 * @constructor
 * @param {TraceLog} traceLog
 */
function TraceLogPlayer(traceLog)
{
    this._traceLog = traceLog;
    this._nextReplayStep = 0;
    this._replayWorldCache = new Cache();
}

TraceLogPlayer.prototype = {
    /**
     * @return {TraceLog}
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
        // FIXME: Prevent memory leaks: detach and delete all old resources OR reuse them OR create a new replay canvas every time.
        this._nextReplayStep = 0;
        this._replayWorldCache.reset();
    },

    step: function()
    {
        this.stepTo(this._nextReplayStep);
    },

    /**
     * @param {number} stepNum
     */
    stepTo: function(stepNum)
    {
        stepNum = Math.min(stepNum, this._traceLog.size() - 1);
        console.assert(stepNum >= 0);
        if (this._nextReplayStep > stepNum)
            this.reset();
        // FIXME: Replay all the cached resources first to warm-up.
        var replayableCalls = this._traceLog.replayableCalls();
        while (this._nextReplayStep <= stepNum)
            replayableCalls[this._nextReplayStep++].replay(this._replayWorldCache);
    },

    replay: function()
    {
        this.stepTo(this._traceLog.size() - 1);
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

    /**
     * @param {Resource} resource
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
    this._lastTraceLogId = 0;
    this._traceLogs = {};
    this._traceLogPlayer = null;
    this._replayContext = null;
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
        var id = this._makeTraceLogId();
        this._manager.captureFrame();
        this._traceLogs[id] = this._manager.lastTraceLog();
        return id;
    },

    /**
     * @param {string} id
     */
    dropTraceLog: function(id)
    {
        if (this._traceLogPlayer && this._traceLogPlayer.traceLog() === this._traceLogs[id])
            this._traceLogPlayer = null;
        delete this._traceLogs[id];
    },

    /**
     * @param {string} id
     * @return {Object|string}
     */
    traceLog: function(id)
    {
        var traceLog = this._traceLogs[id];
        if (!traceLog)
            return "Error: Trace log with this ID not found.";
        var result = {
            id: id,
            calls: []
        };
        var calls = traceLog.replayableCalls();
        for (var i = 0, n = calls.length; i < n; ++i) {
            var call = calls[i];
            result.calls.push({
                functionName: call.functionName() + "(" + call.args().join(", ") + ") => " + call.result()
            });
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
        if (!this._traceLogPlayer || this._traceLogPlayer.traceLog() !== traceLog)
            this._traceLogPlayer = new TraceLogPlayer(traceLog);
        this._traceLogPlayer.stepTo(stepNo);
        if (!this._replayContext) {
            console.error("ASSERT_NOT_REACHED: replayTraceLog failed to create a replay canvas?!");
            return "";
        }
        // Return current screenshot.
        return this._replayContext.canvas.toDataURL();
    },

    /**
     * @return {string}
     */
    _makeTraceLogId: function()
    {
        return "{\"injectedScriptId\":" + injectedScriptId + ",\"traceLogId\":" + (++this._lastTraceLogId) + "}";
    }
}

var injectedScript = new InjectedScript();
return injectedScript;

})
