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
     * @return {Function}
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
            var context = Resource.wrappedObject(result.getContext("2d"));
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
        var context = TypeUtils._dummyCanvas2dContextInstance;
        if (!context) {
            var canvas = inspectedWindow.document.createElement("canvas");
            context = /** @type {CanvasRenderingContext2D} */ Resource.wrappedObject(canvas.getContext("2d"));
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

    this._error = /** @type {{stack: Array}} */ {};
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
    }
}

StackTraceV8.prototype.__proto__ = StackTrace.prototype;

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
 * @param {StackTrace=} stackTrace
 */
function Call(thisObject, functionName, args, result, stackTrace)
{
    this._thisObject = thisObject;
    this._functionName = functionName;
    this._args = Array.prototype.slice.call(args, 0);
    this._result = result;
    this._stackTrace = stackTrace || null;
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
     * @param {Cache} cache
     * @return {ReplayableCall}
     */
    toReplayable: function(cache)
    {
        this.freeze();
        var thisObject = /** @type {ReplayableResource} */ Resource.toReplayable(this._thisObject, cache);
        var result = Resource.toReplayable(this._result, cache);
        var args = this._args.map(function(obj) {
            return Resource.toReplayable(obj, cache);
        });
        return new ReplayableCall(thisObject, this._functionName, args, result, this._stackTrace);
    },

    /**
     * @param {ReplayableCall} replayableCall
     * @param {Cache} cache
     * @return {Call}
     */
    replay: function(replayableCall, cache)
    {
        var replayObject = ReplayableResource.replay(replayableCall.resource(), cache);
        var replayFunction = replayObject[replayableCall.functionName()];
        console.assert(typeof replayFunction === "function", "Expected a function to replay");
        var replayArgs = replayableCall.args().map(function(obj) {
            return ReplayableResource.replay(obj, cache);
        });
        var replayResult = replayFunction.apply(replayObject, replayArgs);
        if (replayableCall.result() instanceof ReplayableResource) {
            var resource = replayableCall.result().replay(cache);
            if (!resource.wrappedObject())
                resource.setWrappedObject(replayResult);
        }

        this._thisObject = replayObject;
        this._functionName = replayableCall.functionName();
        this._args = replayArgs;
        this._result = replayResult;
        this._stackTrace = replayableCall.stackTrace();
        this._freezed = true;
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
 */
function ReplayableCall(thisObject, functionName, args, result, stackTrace)
{
    this._thisObject = thisObject;
    this._functionName = functionName;
    this._args = args;
    this._result = result;
    this._stackTrace = stackTrace;
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
     * @return {StackTrace}
     */
    stackTrace: function()
    {
        return this._stackTrace;
    },

    /**
     * @param {Cache} cache
     * @return {Call}
     */
    replay: function(cache)
    {
        var call = Object.create(Call.prototype);
        return call.replay(this, cache);
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
 * @param {Cache} cache
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
     * @param {Cache} cache
     * @return {ReplayableResource}
     */
    toReplayable: function(cache)
    {
        var result = cache.get(this._id);
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
     * @param {Object} data
     * @param {Cache} cache
     */
    _populateReplayableData: function(data, cache)
    {
        // Do nothing. Should be overridden by subclasses.
    },

    /**
     * @param {Object} data
     * @param {Cache} cache
     * @return {Resource}
     */
    replay: function(data, cache)
    {
        var resource = cache.get(data.id);
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
     * @param {Object} data
     * @param {Cache} cache
     */
    _doReplayCalls: function(data, cache)
    {
        for (var i = 0, n = data.calls.length; i < n; ++i)
            this._calls.push(data.calls[i].replay(cache));
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
    }
}

/**
 * @constructor
 * @param {Resource} originalResource
 * @param {Object} data
 */
function ReplayableResource(originalResource, data)
{
    this._proto = originalResource.__proto__;
    this._data = data;
}

ReplayableResource.prototype = {
    /**
     * @param {Cache} cache
     * @return {Resource}
     */
    replay: function(cache)
    {
        var result = Object.create(this._proto);
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
function WebGLBoundResource(wrappedObject)
{
    Resource.call(this, wrappedObject);
    this._state = {};
}

WebGLBoundResource.prototype = {
    /**
     * @override
     * @param {Object} data
     * @param {Cache} cache
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
     * @param {Object} data
     * @param {Cache} cache
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
    /**
     * @override
     * @param {Object} data
     * @param {Cache} cache
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
     * @param {Call} call
     */
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
    /**
     * @override
     * @param {Object} data
     * @param {Cache} cache
     */
    _populateReplayableData: function(data, cache)
    {
        var glResource = WebGLRenderingContextResource.forObject(this);
        var gl = glResource.wrappedObject();
        var program = this.wrappedObject();

        var uniforms = [];
        var uniformsCount = gl.getProgramParameter(program, gl.ACTIVE_UNIFORMS);
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
    },

    /**
     * @override
     * @param {Object} data
     * @param {Cache} cache
     */
    _doReplayCalls: function(data, cache)
    {
        Resource.prototype._doReplayCalls.call(this, data, cache);
        var gl = WebGLRenderingContextResource.forObject(this).wrappedObject();
        var program = this.wrappedObject();

        var originalProgram = gl.getParameter(gl.CURRENT_PROGRAM);
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
     * @param {Call} call
     */
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
    /**
     * @override
     * @param {Call} call
     */
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
    /**
     * @override
     * @param {Call} call
     */
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
    /**
     * @override
     * @param {Call} call
     */
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
    /**
     * @override
     * @param {Call} call
     */
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
 * @param {Function} replayContextCallback
 */
function WebGLRenderingContextResource(glContext, replayContextCallback)
{
    Resource.call(this, glContext);
    this._replayContextCallback = replayContextCallback;
    this._proxyObject = null;
    /** @type {Object.<number, boolean>} */
    this._customErrors = null;
}

/**
 * @const
 * @type {Array.<string>}
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
 * @const
 * @type {Array.<string>}
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
     * @override
     * @param {Object} data
     * @param {Cache} cache
     */
    _populateReplayableData: function(data, cache)
    {
        var gl = this.wrappedObject();
        data.replayContextCallback = this._replayContextCallback;

        // FIXME: Save the getError() status and restore it after taking the GL state snapshot.

        // Take a full GL state snapshot.
        var glState = {};
        WebGLRenderingContextResource.GLCapabilities.forEach(function(parameter) {
            glState[parameter] = gl.isEnabled(gl[parameter]);
        });
        WebGLRenderingContextResource.StateParameters.forEach(function(parameter) {
            glState[parameter] = Resource.toReplayable(gl.getParameter(gl[parameter]), cache);
            // FIXME: Call while(gl.getError() != gl.NO_ERROR) {...} to check if a particular parameter is supported.
        });

        // VERTEX_ATTRIB_ARRAYS
        var maxVertexAttribs = gl.getParameter(gl.MAX_VERTEX_ATTRIBS);
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
        var currentTextureBinding = gl.getParameter(gl.ACTIVE_TEXTURE);
        var maxTextureImageUnits = gl.getParameter(gl.MAX_TEXTURE_IMAGE_UNITS);
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
    },

    /**
     * @override
     * @param {Object} data
     * @param {Cache} cache
     */
    _doReplayCalls: function(data, cache)
    {
        this._replayContextCallback = data.replayContextCallback;
        this._proxyObject = null;
        this._customErrors = null;

        var gl = Resource.wrappedObject(this._replayContextCallback());
        this.setWrappedObject(gl);

        var glState = data.glState;
        gl.bindFramebuffer(gl.FRAMEBUFFER, ReplayableResource.replay(glState.FRAMEBUFFER_BINDING, cache));
        gl.bindRenderbuffer(gl.RENDERBUFFER, ReplayableResource.replay(glState.RENDERBUFFER_BINDING, cache));

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

        gl.useProgram(ReplayableResource.replay(glState.CURRENT_PROGRAM, cache));

        // VERTEX_ATTRIB_ARRAYS
        var maxVertexAttribs = gl.getParameter(gl.MAX_VERTEX_ATTRIBS);
        for (var i = 0; i < maxVertexAttribs; ++i) {
            var state = glState.vertexAttribStates[i] || {};
            if (state.VERTEX_ATTRIB_ARRAY_ENABLED)
                gl.enableVertexAttribArray(i);
            else
                gl.disableVertexAttribArray(i);
            if (state.CURRENT_VERTEX_ATTRIB)
                gl.vertexAttrib4fv(i, state.CURRENT_VERTEX_ATTRIB);
            var buffer = ReplayableResource.replay(state.VERTEX_ATTRIB_ARRAY_BUFFER_BINDING, cache);
            if (buffer) {
                gl.bindBuffer(gl.ARRAY_BUFFER, buffer);
                gl.vertexAttribPointer(i, state.VERTEX_ATTRIB_ARRAY_SIZE, state.VERTEX_ATTRIB_ARRAY_TYPE, state.VERTEX_ATTRIB_ARRAY_NORMALIZED, state.VERTEX_ATTRIB_ARRAY_STRIDE, state.VERTEX_ATTRIB_ARRAY_POINTER);
            }
        }
        gl.bindBuffer(gl.ARRAY_BUFFER, ReplayableResource.replay(glState.ARRAY_BUFFER_BINDING, cache));
        gl.bindBuffer(gl.ELEMENT_ARRAY_BUFFER, ReplayableResource.replay(glState.ELEMENT_ARRAY_BUFFER_BINDING, cache));

        // TEXTURES
        var maxTextureImageUnits = gl.getParameter(gl.MAX_TEXTURE_IMAGE_UNITS);
        for (var i = 0; i < maxTextureImageUnits; ++i) {
            gl.activeTexture(gl.TEXTURE0 + i);
            var state = glState.textureBindings[i] || {};
            gl.bindTexture(gl.TEXTURE_2D, ReplayableResource.replay(state.TEXTURE_2D, cache));
            gl.bindTexture(gl.TEXTURE_CUBE_MAP, ReplayableResource.replay(state.TEXTURE_CUBE_MAP, cache));
        }
        gl.activeTexture(glState.ACTIVE_TEXTURE);

        return Resource.prototype._doReplayCalls.call(this, data, cache);
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
        var gl = this.wrappedObject();
        var proxy = Object.create(gl.__proto__); // In order to emulate "instanceof".

        var self = this;
        var customWrapFunctions = WebGLRenderingContextResource.wrapFunctions();
        function processProperty(property)
        {
            if (typeof gl[property] === "function") {
                var customWrapFunction = customWrapFunctions[property];
                if (customWrapFunction)
                    proxy[property] = self._wrapCustomFunction(self, gl, gl[property], property, customWrapFunction);
                else
                    proxy[property] = self._wrapFunction(self, gl, gl[property], property);
            } else if (/^[A-Z0-9_]+$/.test(property)) {
                // Fast access to enums and constants.
                console.assert(typeof gl[property] === "number", "Expected a number for property " + property);
                proxy[property] = gl[property];
            } else {
                Object.defineProperty(proxy, property, {
                    get: function()
                    {
                        return gl[property];
                    },
                    set: function(value)
                    {
                        console.error("ASSERT_NOT_REACHED: We assume all WebGLRenderingContext attributes are readonly. Trying to mutate " + property);
                        gl[property] = value;
                    }
                });
            }
        }

        for (var property in gl)
            processProperty(property);

        this._bindObjectToResource(proxy);
        return proxy;
    },

    /**
     * @param {Resource} resource
     * @param {WebGLRenderingContext} originalObject
     * @param {Function} originalFunction
     * @param {string} functionName
     * @param {Function} customWrapFunction
     * @return {Function}
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
            if (manager && manager.capturing()) {
                var call = wrapFunction.call();
                call.setStackTrace(StackTrace.create(1, arguments.callee));
                manager.reportCall(call);
            }
            return wrapFunction.result();
        };
    },

    /**
     * @param {Resource} resource
     * @param {WebGLRenderingContext} originalObject
     * @param {Function} originalFunction
     * @param {string} functionName
     * @return {Function}
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

/**
 * @return {Object.<string, Function>}
 */
WebGLRenderingContextResource.wrapFunctions = function()
{
    var wrapFunctions = WebGLRenderingContextResource._wrapFunctions;
    if (!wrapFunctions) {
        wrapFunctions = Object.create(null);

        /**
         * @param {string} methodName
         * @param {Function} resourceConstructor
         */
        function createResourceWrapFunction(methodName, resourceConstructor)
        {
            /** @this WebGLRenderingContextResource.WrapFunction */
            wrapFunctions[methodName] = function()
            {
                var wrappedObject = this.result();
                if (!wrappedObject)
                    return;
                var resource = new resourceConstructor(wrappedObject);
                var manager = this._glResource.manager();
                if (manager)
                    manager.registerResource(resource);
                resource.pushCall(this.call());
            }
        }
        createResourceWrapFunction("createBuffer", WebGLBufferResource);
        createResourceWrapFunction("createShader", WebGLShaderResource);
        createResourceWrapFunction("createProgram", WebGLProgramResource);
        createResourceWrapFunction("createTexture", WebGLTextureResource);
        createResourceWrapFunction("createFramebuffer", WebGLFramebufferResource);
        createResourceWrapFunction("createRenderbuffer", WebGLRenderbufferResource);
        createResourceWrapFunction("getUniformLocation", Resource);

        /**
         * @param {string} methodName
         */
        function customWrapFunction(methodName)
        {
            var customPushCall = "pushCall_" + methodName;
            /**
             * @param {Object|number} target
             * @this WebGLRenderingContextResource.WrapFunction
             */
            wrapFunctions[methodName] = function(target)
            {
                var resource = this._glResource.currentBinding(target);
                if (!resource)
                    return;
                if (resource[customPushCall])
                    resource[customPushCall].call(resource, this.call());
                else
                    resource.pushCall(this.call());
            }
        }
        customWrapFunction("attachShader");
        customWrapFunction("bindAttribLocation");
        customWrapFunction("compileShader");
        customWrapFunction("detachShader");
        customWrapFunction("linkProgram");
        customWrapFunction("shaderSource");
        customWrapFunction("bufferData");
        customWrapFunction("bufferSubData");
        customWrapFunction("compressedTexImage2D");
        customWrapFunction("compressedTexSubImage2D");
        customWrapFunction("copyTexImage2D");
        customWrapFunction("copyTexSubImage2D");
        customWrapFunction("generateMipmap");
        customWrapFunction("texImage2D");
        customWrapFunction("texSubImage2D");
        customWrapFunction("texParameterf");
        customWrapFunction("texParameteri");
        customWrapFunction("framebufferRenderbuffer");
        customWrapFunction("framebufferTexture2D");
        customWrapFunction("renderbufferStorage");

        WebGLRenderingContextResource._wrapFunctions = wrapFunctions;
    }
    return wrapFunctions;
}

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
        resource.toReplayable(this._replayablesCache);
    },

    /**
     * @param {Call} call
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
        var resource = Resource.forObject(glContext) || new WebGLRenderingContextResource(glContext, this._constructWebGLReplayContext.bind(this, glContext));
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
            var args = call.args().map(function(argument) {
                return argument + "";
            });
            var stackTrace = call.stackTrace();
            var callFrame = stackTrace ? stackTrace.callFrame(0) || {} : {};
            var traceLogItem = {
                functionName: call.functionName(),
                arguments: args,
                sourceURL: callFrame.sourceURL,
                lineNumber: callFrame.lineNumber,
                columnNumber: callFrame.columnNumber
            };
            if (call.result())
                traceLogItem.result = call.result() + "";
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
        if (!this._traceLogPlayer || this._traceLogPlayer.traceLog() !== traceLog)
            this._traceLogPlayer = new TraceLogPlayer(traceLog);
        this._traceLogPlayer.stepTo(stepNo);
        if (!this._replayContext) {
            console.error("ASSERT_NOT_REACHED: replayTraceLog failed to create a replay canvas?!");
            return "";
        }
        // Return current screenshot.
        // FIXME: Support replaying several canvases simultaneously.
        return this._replayContext.canvas.toDataURL();
    },

    /**
     * @return {string}
     */
    _makeTraceLogId: function()
    {
        return "{\"injectedScriptId\":" + injectedScriptId + ",\"traceLogId\":" + (++this._lastTraceLogId) + "}";
    },

    /**
     * @param {WebGLRenderingContext} originalGlContext
     * @return {WebGLRenderingContext}
     */
    _constructWebGLReplayContext: function(originalGlContext)
    {
        var replayContext = originalGlContext["__replayContext"];
        if (!replayContext) {
            var canvas = originalGlContext.canvas.cloneNode(true);
            var attributes = originalGlContext.getContextAttributes();
            var contextIds = ["experimental-webgl", "webkit-3d", "3d"];
            for (var i = 0, contextId; contextId = contextIds[i]; ++i) {
                replayContext = canvas.getContext(contextId, attributes);
                if (replayContext) {
                    replayContext = /** @type {WebGLRenderingContext} */ Resource.wrappedObject(replayContext);
                    break;
                }
            }
            Object.defineProperty(originalGlContext, "__replayContext", {
                value: replayContext,
                writable: false,
                enumerable: false,
                configurable: true
            });
            this._replayContext = replayContext;
        } else {
            // FIXME: Reset the replay GL state and clear the canvas.
        }
        return replayContext;
    }
}

var injectedScript = new InjectedScript();
return injectedScript;

})
