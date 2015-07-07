/*
 * Copyright (C) 2009 Google Inc. All rights reserved.
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

WebInspector.RemoteObject = function(objectId, type, subtype, value, description)
{
    this._type = type;
    this._subtype = subtype;
    if (objectId) {
        // handle
        this._objectId = objectId;
        this._description = description;
        this._hasChildren = true;
    } else {
        // Primitive or null object.
        console.assert(type !== "object" || value === null);
        this._description = description || (value + "");
        this._hasChildren = false;
        this.value = value;
    }
}

WebInspector.RemoteObject.fromPrimitiveValue = function(value)
{
    return new WebInspector.RemoteObject(undefined, typeof value, undefined, value);
}

WebInspector.RemoteObject.fromLocalObject = function(value)
{
    return new WebInspector.LocalJSONObject(value);
}

WebInspector.RemoteObject.resolveNode = function(node, objectGroup, callback)
{
    function mycallback(error, object)
    {
        if (!callback)
            return;

        if (error || !object)
            callback(null);
        else
            callback(WebInspector.RemoteObject.fromPayload(object));
    }
    DOMAgent.resolveNode(node.id, objectGroup, mycallback);
}

WebInspector.RemoteObject.fromPayload = function(payload)
{
    console.assert(typeof payload === "object", "Remote object payload should only be an object");

    return new WebInspector.RemoteObject(payload.objectId, payload.type, payload.subtype, payload.value, payload.description);
}

WebInspector.RemoteObject.type = function(remoteObject)
{
    if (remoteObject === null)
        return "null";

    var type = typeof remoteObject;
    if (type !== "object" && type !== "function")
        return type;

    return remoteObject.type;
}

WebInspector.RemoteObject.prototype = {
    get objectId()
    {
        return this._objectId;
    },

    get type()
    {
        return this._type;
    },

    get subtype()
    {
        return this._subtype;
    },

    get description()
    {
        return this._description;
    },

    get hasChildren()
    {
        return this._hasChildren;
    },

    getOwnProperties: function(callback)
    {
        this._getProperties(true, false, callback);
    },

    getOwnAndGetterProperties: function(callback)
    {
        this._getProperties(false, true, callback);
    },

    getAllProperties: function(callback)
    {
        this._getProperties(false, false, callback);
    },

    _getProperties: function(ownProperties, ownAndGetterProperties, callback)
    {
        if (!this._objectId) {
            callback([]);
            return;
        }

        function remoteObjectBinder(error, properties)
        {
            if (error) {
                callback(null);
                return;
            }
            var result = [];
            for (var i = 0; properties && i < properties.length; ++i) {
                var property = properties[i];
                if (property.get || property.set) {
                    if (property.get)
                        result.push(new WebInspector.RemoteObjectProperty("get " + property.name, WebInspector.RemoteObject.fromPayload(property.get), property));
                    if (property.set)
                        result.push(new WebInspector.RemoteObjectProperty("set " + property.name, WebInspector.RemoteObject.fromPayload(property.set), property));
                } else
                    result.push(new WebInspector.RemoteObjectProperty(property.name, WebInspector.RemoteObject.fromPayload(property.value), property));
            }
            callback(result);
        }

        // COMPATIBILITY (iOS 8): RuntimeAgent.getProperties did not support ownerAndGetterProperties.
        // Here we do our best to reimplement it by getting all properties and reducing them down.
        if (ownAndGetterProperties && !RuntimeAgent.getProperties.supports("ownAndGetterProperties")) {
            RuntimeAgent.getProperties(this._objectId, function(error, allProperties) {
                var ownOrGetterPropertiesList = [];
                if (allProperties) {
                    for (var property of allProperties) {
                        if (property.isOwn || property.get || property.name === "__proto__") {
                            // Own property or getter property in prototype chain.
                            ownOrGetterPropertiesList.push(property);
                        } else if (property.value && property.name !== property.name.toUpperCase()) {
                            var type = property.value.type;
                            if (type && type !== "function" && property.name !== "constructor") {
                                // Possible native binding getter property converted to a value. Also, no CONSTANT name style and not "constructor".
                                ownOrGetterPropertiesList.push(property);
                            }
                        }
                    }
                }
                remoteObjectBinder(error, ownOrGetterPropertiesList);
            }); 
            return;
        }

        RuntimeAgent.getProperties(this._objectId, ownProperties, ownAndGetterProperties, remoteObjectBinder);
    },

    setPropertyValue: function(name, value, callback)
    {
        if (!this._objectId) {
            callback("Can't set a property of non-object.");
            return;
        }

        RuntimeAgent.evaluate.invoke({expression:value, doNotPauseOnExceptionsAndMuteConsole:true}, evaluatedCallback.bind(this));

        function evaluatedCallback(error, result, wasThrown)
        {
            if (error || wasThrown) {
                callback(error || result.description);
                return;
            }

            function setPropertyValue(propertyName, propertyValue)
            {
                this[propertyName] = propertyValue;
            }

            delete result.description; // Optimize on traffic.
            RuntimeAgent.callFunctionOn(this._objectId, setPropertyValue.toString(), [{ value:name }, result], true, undefined, propertySetCallback.bind(this));
            if (result._objectId)
                RuntimeAgent.releaseObject(result._objectId);
        }

        function propertySetCallback(error, result, wasThrown)
        {
            if (error || wasThrown) {
                callback(error || result.description);
                return;
            }
            callback();
        }
    },

    pushNodeToFrontend: function(callback)
    {
        if (this._objectId)
            WebInspector.domTreeManager.pushNodeToFrontend(this._objectId, callback);
        else
            callback(0);
    },

    callFunction: function(functionDeclaration, args, callback)
    {
        function mycallback(error, result, wasThrown)
        {
            callback((error || wasThrown) ? null : WebInspector.RemoteObject.fromPayload(result));
        }

        RuntimeAgent.callFunctionOn(this._objectId, functionDeclaration.toString(), args, true, undefined, mycallback);
    },

    callFunctionJSON: function(functionDeclaration, args, callback)
    {
        function mycallback(error, result, wasThrown)
        {
            callback((error || wasThrown) ? null : result.value);
        }

        RuntimeAgent.callFunctionOn(this._objectId, functionDeclaration.toString(), args, true, true, mycallback);
    },

    release: function()
    {
        RuntimeAgent.releaseObject(this._objectId);
    },

    arrayLength: function()
    {
        if (this.subtype !== "array")
            return 0;

        var matches = this._description.match(/\[([0-9]+)\]/);
        if (!matches)
            return 0;
        return parseInt(matches[1], 10);
    }
}

WebInspector.RemoteObjectProperty = function(name, value, descriptor)
{
    this.name = name;
    this.value = value;
    this.enumerable = descriptor ? !!descriptor.enumerable : true;
    this.writable = descriptor ? !!descriptor.writable : true;
    if (descriptor && descriptor.wasThrown)
        this.wasThrown = true;
}

WebInspector.RemoteObjectProperty.fromPrimitiveValue = function(name, value)
{
    return new WebInspector.RemoteObjectProperty(name, WebInspector.RemoteObject.fromPrimitiveValue(value));
}

// The below is a wrapper around a local object that provides an interface comaptible
// with RemoteObject, to be used by the UI code (primarily ObjectPropertiesSection).
// Note that only JSON-compliant objects are currently supported, as there's no provision
// for traversing prototypes, extracting class names via constuctor, handling properties
// or functions.

WebInspector.LocalJSONObject = function(value)
{
    this._value = value;
}

WebInspector.LocalJSONObject.prototype = {
    get description()
    {
        if (this._cachedDescription)
            return this._cachedDescription;

        if (this.type === "object") {
            switch (this.subtype) {
            case "array":
                function formatArrayItem(property)
                {
                    return property.value.description;
                }
                this._cachedDescription = this._concatenate("[", "]", formatArrayItem);
                break;
            case "null":
                this._cachedDescription = "null";
                break;
            default:
                function formatObjectItem(property)
                {
                    return property.name + ":" + property.value.description;
                }
                this._cachedDescription = this._concatenate("{", "}", formatObjectItem);
            }
        } else
            this._cachedDescription = String(this._value);

        return this._cachedDescription;
    },

    _concatenate: function(prefix, suffix, formatProperty)
    {
        const previewChars = 100;

        var buffer = prefix;
        var children = this._children();
        for (var i = 0; i < children.length; ++i) {
            var itemDescription = formatProperty(children[i]);
            if (buffer.length + itemDescription.length > previewChars) {
                buffer += ",\u2026";
                break;
            }
            if (i)
                buffer += ", ";
            buffer += itemDescription;
        }
        buffer += suffix;
        return buffer;
    },

    get type()
    {
        return typeof this._value;
    },

    get subtype()
    {
        if (this._value === null)
            return "null";

        if (this._value instanceof Array)
            return "array";

        return undefined;
    },

    get hasChildren()
    {
        return typeof this._value === "object" && this._value !== null && !isEmptyObject(this._value);
    },

    getOwnProperties: function(callback)
    {
        callback(this._children());
    },

    getAllProperties: function(callback)
    {
        callback(this._children());
    },

    _children: function()
    {
        if (!this.hasChildren)
            return [];

        function buildProperty(propName)
        {
            return new WebInspector.RemoteObjectProperty(propName, new WebInspector.LocalJSONObject(this._value[propName]));
        }
        if (!this._cachedChildren)
            this._cachedChildren = Object.keys(this._value || {}).map(buildProperty.bind(this));
        return this._cachedChildren;
    },

    isError: function()
    {
        return false;
    }
}
