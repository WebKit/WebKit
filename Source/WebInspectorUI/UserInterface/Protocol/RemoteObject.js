/*
 * Copyright (C) 2009 Google Inc. All rights reserved.
 * Copyright (C) 2015 Apple Inc. All rights reserved.
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

WI.RemoteObject = class RemoteObject
{
    constructor(target, objectId, type, subtype, value, description, size, classPrototype, className, preview)
    {
        console.assert(type);
        console.assert(!preview || preview instanceof WI.ObjectPreview);
        console.assert(!target || target instanceof WI.Target);

        this._target = target || WI.mainTarget;
        this._type = type;
        this._subtype = subtype;

        if (objectId) {
            // Object, Function, or Symbol.
            console.assert(!subtype || typeof subtype === "string");
            console.assert(!description || typeof description === "string");
            console.assert(!value);

            this._objectId = objectId;
            this._description = description || "";
            this._hasChildren = type !== "symbol";
            this._size = size;
            this._classPrototype = classPrototype;
            this._preview = preview;

            if (subtype === "class") {
                this._functionDescription = this._description;
                this._description = "class " + className;
            }
        } else {
            // Primitive or null.
            console.assert(type !== "object" || value === null);
            console.assert(!preview);

            this._description = description || (value + "");
            this._hasChildren = false;
            this._value = value;
        }
    }

    // Static

    static createFakeRemoteObject()
    {
        return new WI.RemoteObject(undefined, WI.RemoteObject.FakeRemoteObjectId, "object");
    }

    static fromPrimitiveValue(value)
    {
        return new WI.RemoteObject(undefined, undefined, typeof value, undefined, value, undefined, undefined, undefined, undefined);
    }

    static fromPayload(payload, target)
    {
        console.assert(typeof payload === "object", "Remote object payload should only be an object");

        if (payload.subtype === "array") {
            // COMPATIBILITY (iOS 8): Runtime.RemoteObject did not have size property,
            // instead it was tacked onto the end of the description, like "Array[#]".
            var match = payload.description.match(/\[(\d+)\]$/);
            if (match) {
                payload.size = parseInt(match[1]);
                payload.description = payload.description.replace(/\[\d+\]$/, "");
            }
        }

        if (payload.classPrototype)
            payload.classPrototype = WI.RemoteObject.fromPayload(payload.classPrototype, target);

        if (payload.preview) {
            // COMPATIBILITY (iOS 8): Did not have type/subtype/description on
            // Runtime.ObjectPreview. Copy them over from the RemoteObject.
            if (!payload.preview.type) {
                payload.preview.type = payload.type;
                payload.preview.subtype = payload.subtype;
                payload.preview.description = payload.description;
                payload.preview.size = payload.size;
            }

            payload.preview = WI.ObjectPreview.fromPayload(payload.preview);
        }

        return new WI.RemoteObject(target, payload.objectId, payload.type, payload.subtype, payload.value, payload.description, payload.size, payload.classPrototype, payload.className, payload.preview);
    }

    static createCallArgument(valueOrObject)
    {
        if (valueOrObject instanceof WI.RemoteObject) {
            if (valueOrObject.objectId)
                return {objectId: valueOrObject.objectId};
            return {value: valueOrObject.value};
        }

        return {value: valueOrObject};
    }

    static resolveNode(node, objectGroup)
    {
        return DOMAgent.resolveNode(node.id, objectGroup)
            .then(({object}) => WI.RemoteObject.fromPayload(object, WI.mainTarget));
    }

    static resolveWebSocket(webSocketResource, objectGroup, callback)
    {
        console.assert(typeof callback === "function");

        NetworkAgent.resolveWebSocket(webSocketResource.requestIdentifier, objectGroup, (error, object) => {
            if (error || !object)
                callback(null);
            else
                callback(WI.RemoteObject.fromPayload(object, webSocketResource.target));
        });
    }

    static resolveCanvasContext(canvas, objectGroup, callback)
    {
        console.assert(typeof callback === "function");

        CanvasAgent.resolveCanvasContext(canvas.identifier, objectGroup, (error, object) => {
            if (error || !object)
                callback(null);
            else
                callback(WI.RemoteObject.fromPayload(object, WI.mainTarget));
        });
    }

    // Public

    get target()
    {
        return this._target;
    }

    get objectId()
    {
        return this._objectId;
    }

    get type()
    {
        return this._type;
    }

    get subtype()
    {
        return this._subtype;
    }

    get description()
    {
        return this._description;
    }

    get functionDescription()
    {
        console.assert(this.type === "function");

        return this._functionDescription || this._description;
    }

    get hasChildren()
    {
        return this._hasChildren;
    }

    get value()
    {
        return this._value;
    }

    get size()
    {
        return this._size || 0;
    }

    get classPrototype()
    {
        return this._classPrototype;
    }

    get preview()
    {
        return this._preview;
    }

    hasSize()
    {
        return this.isArray() || this.isCollectionType();
    }

    hasValue()
    {
        return "_value" in this;
    }

    canLoadPreview()
    {
        if (this._failedToLoadPreview)
            return false;

        if (this._type !== "object")
            return false;

        if (!this._objectId || this._isSymbol() || this._isFakeObject())
            return false;

        return true;
    }

    updatePreview(callback)
    {
        if (!this.canLoadPreview()) {
            callback(null);
            return;
        }

        if (!RuntimeAgent.getPreview) {
            this._failedToLoadPreview = true;
            callback(null);
            return;
        }

        this._target.RuntimeAgent.getPreview(this._objectId, (error, payload) => {
            if (error) {
                this._failedToLoadPreview = true;
                callback(null);
                return;
            }

            this._preview = WI.ObjectPreview.fromPayload(payload);
            callback(this._preview);
        });
    }

    getPropertyDescriptors(callback, options = {})
    {
        if (!this._objectId || this._isSymbol() || this._isFakeObject()) {
            callback([]);
            return;
        }

        this._target.RuntimeAgent.getProperties(this._objectId, options.ownProperties, options.generatePreview, this._getPropertyDescriptorsResolver.bind(this, callback));
    }

    getDisplayablePropertyDescriptors(callback)
    {
        if (!this._objectId || this._isSymbol() || this._isFakeObject()) {
            callback([]);
            return;
        }

        // COMPATIBILITY (iOS 8): RuntimeAgent.getDisplayableProperties did not exist.
        // Here we do our best to reimplement it by getting all properties and reducing them down.
        if (!RuntimeAgent.getDisplayableProperties) {
            this._target.RuntimeAgent.getProperties(this._objectId, function(error, allProperties) {
                var ownOrGetterPropertiesList = [];
                if (allProperties) {
                    for (var property of allProperties) {
                        if (property.isOwn || property.name === "__proto__") {
                            // Own property or getter property in prototype chain.
                            ownOrGetterPropertiesList.push(property);
                        } else if (property.value && property.name !== property.name.toUpperCase()) {
                            var type = property.value.type;
                            if (type && type !== "function" && property.name !== "constructor") {
                                // Possible native binding getter property converted to a value. Also, no CONSTANT name style and not "constructor".
                                // There is no way of knowing if this is native or not, so just go with it.
                                ownOrGetterPropertiesList.push(property);
                            }
                        }
                    }
                }

                this._getPropertyDescriptorsResolver(callback, error, ownOrGetterPropertiesList);
            }.bind(this));
            return;
        }

        this._target.RuntimeAgent.getDisplayableProperties(this._objectId, true, this._getPropertyDescriptorsResolver.bind(this, callback));
    }

    // FIXME: Phase out these deprecated functions. They return DeprecatedRemoteObjectProperty instead of PropertyDescriptors.
    deprecatedGetOwnProperties(callback)
    {
        this._deprecatedGetProperties(true, callback);
    }

    deprecatedGetAllProperties(callback)
    {
        this._deprecatedGetProperties(false, callback);
    }

    deprecatedGetDisplayableProperties(callback)
    {
        if (!this._objectId || this._isSymbol() || this._isFakeObject()) {
            callback([]);
            return;
        }

        // COMPATIBILITY (iOS 8): RuntimeAgent.getProperties did not support ownerAndGetterProperties.
        // Here we do our best to reimplement it by getting all properties and reducing them down.
        if (!RuntimeAgent.getDisplayableProperties) {
            this._target.RuntimeAgent.getProperties(this._objectId, function(error, allProperties) {
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

                this._deprecatedGetPropertiesResolver(callback, error, ownOrGetterPropertiesList);
            }.bind(this));
            return;
        }

        this._target.RuntimeAgent.getDisplayableProperties(this._objectId, this._deprecatedGetPropertiesResolver.bind(this, callback));
    }

    setPropertyValue(name, value, callback)
    {
        if (!this._objectId || this._isSymbol() || this._isFakeObject()) {
            callback("Can't set a property of non-object.");
            return;
        }

        // FIXME: It doesn't look like setPropertyValue is used yet. This will need to be tested when it is again (editable ObjectTrees).
        this._target.RuntimeAgent.evaluate.invoke({expression: appendWebInspectorSourceURL(value), doNotPauseOnExceptionsAndMuteConsole: true}, evaluatedCallback.bind(this), this._target.RuntimeAgent);

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

            this._target.RuntimeAgent.callFunctionOn(this._objectId, appendWebInspectorSourceURL(setPropertyValue.toString()), [{value: name}, result], true, undefined, propertySetCallback.bind(this));

            if (result._objectId)
                this._target.RuntimeAgent.releaseObject(result._objectId);
        }

        function propertySetCallback(error, result, wasThrown)
        {
            if (error || wasThrown) {
                callback(error || result.description);
                return;
            }

            callback();
        }
    }

    isUndefined()
    {
        return this._type === "undefined";
    }

    isNode()
    {
        return this._subtype === "node";
    }

    isArray()
    {
        return this._subtype === "array";
    }

    isClass()
    {
        return this._subtype === "class";
    }

    isCollectionType()
    {
        return this._subtype === "map" || this._subtype === "set" || this._subtype === "weakmap" || this._subtype === "weakset";
    }

    isWeakCollection()
    {
        return this._subtype === "weakmap" || this._subtype === "weakset";
    }

    getCollectionEntries(start, numberToFetch, callback)
    {
        start = typeof start === "number" ? start : 0;
        numberToFetch = typeof numberToFetch === "number" ? numberToFetch : 100;

        console.assert(start >= 0);
        console.assert(numberToFetch >= 0);
        console.assert(this.isCollectionType());

        // WeakMaps and WeakSets are not ordered. We should never send a non-zero start.
        console.assert((this._subtype === "weakmap" && start === 0) || this._subtype !== "weakmap");
        console.assert((this._subtype === "weakset" && start === 0) || this._subtype !== "weakset");

        let objectGroup = this.isWeakCollection() ? this._weakCollectionObjectGroup() : "";

        this._target.RuntimeAgent.getCollectionEntries(this._objectId, objectGroup, start, numberToFetch, (error, entries) => {
            entries = entries.map((x) => WI.CollectionEntry.fromPayload(x, this._target));
            callback(entries);
        });
    }

    releaseWeakCollectionEntries()
    {
        console.assert(this.isWeakCollection());

        this._target.RuntimeAgent.releaseObjectGroup(this._weakCollectionObjectGroup());
    }

    pushNodeToFrontend(callback)
    {
        if (this._objectId)
            WI.domManager.pushNodeToFrontend(this._objectId, callback);
        else
            callback(0);
    }

    async fetchProperties(propertyNames, resultObject={})
    {
        let seenPropertyNames = new Set;
        let requestedValues = [];
        for (let propertyName of propertyNames) {
            // Check this here, otherwise things like '{}' would be valid Set keys.
            if (typeof propertyName !== "string" && typeof propertyName !== "number")
                throw new Error(`Tried to get property using key is not a string or number: ${propertyName}`);

            if (seenPropertyNames.has(propertyName))
                continue;

            seenPropertyNames.add(propertyName);
            requestedValues.push(this.getProperty(propertyName));
        }

        // Return primitive values directly, otherwise return a WI.RemoteObject instance.
        function maybeUnwrapValue(remoteObject) {
            return remoteObject.hasValue() ? remoteObject.value : remoteObject;
        }

        // Request property values one by one, since returning an array of property
        // values would then be subject to arbitrary object preview size limits.
        let fetchedKeys = Array.from(seenPropertyNames);
        let fetchedValues = await Promise.all(requestedValues);
        for (let i = 0; i < fetchedKeys.length; ++i)
            resultObject[fetchedKeys[i]] = maybeUnwrapValue(fetchedValues[i]);

        return resultObject;
    }

    getProperty(propertyName, callback = null)
    {
        function inspectedPage_object_getProperty(property) {
            if (typeof property !== "string" && typeof property !== "number")
                throw new Error(`Tried to get property using key is not a string or number: ${property}`);

            return this[property];
        }

        if (callback && typeof callback === "function")
            this.callFunction(inspectedPage_object_getProperty, [propertyName], true, callback);
        else
            return this.callFunction(inspectedPage_object_getProperty, [propertyName], true);
    }

    callFunction(functionDeclaration, args, generatePreview, callback = null)
    {
        let translateResult = (result) => result ? WI.RemoteObject.fromPayload(result, this._target) : null;

        if (args)
            args = args.map(WI.RemoteObject.createCallArgument);

        if (callback && typeof callback === "function") {
            this._target.RuntimeAgent.callFunctionOn(this._objectId, appendWebInspectorSourceURL(functionDeclaration.toString()), args, true, undefined, !!generatePreview, (error, result, wasThrown) => {
                callback(error, translateResult(result), wasThrown);
            });
        } else {
            // Protocol errors and results that were thrown should cause promise rejection with the same.
            return this._target.RuntimeAgent.callFunctionOn(this._objectId, appendWebInspectorSourceURL(functionDeclaration.toString()), args, true, undefined, !!generatePreview)
                .then(({result, wasThrown}) => {
                    result = translateResult(result);
                    if (result && wasThrown)
                        return Promise.reject(result);
                    return Promise.resolve(result);
                });
        }
    }

    callFunctionJSON(functionDeclaration, args, callback)
    {
        function mycallback(error, result, wasThrown)
        {
            callback((error || wasThrown) ? null : result.value);
        }

        this._target.RuntimeAgent.callFunctionOn(this._objectId, appendWebInspectorSourceURL(functionDeclaration.toString()), args, true, true, mycallback);
    }

    invokeGetter(getterRemoteObject, callback)
    {
        console.assert(getterRemoteObject instanceof WI.RemoteObject);

        function backendInvokeGetter(getter)
        {
            return getter ? getter.call(this) : undefined;
        }

        this.callFunction(backendInvokeGetter, [getterRemoteObject], true, callback);
    }

    getOwnPropertyDescriptor(propertyName, callback)
    {
        function backendGetOwnPropertyDescriptor(propertyName)
        {
            return this[propertyName];
        }

        function wrappedCallback(error, result, wasThrown)
        {
            if (error || wasThrown || !(result instanceof WI.RemoteObject)) {
                callback(null);
                return;
            }

            var fakeDescriptor = {name: propertyName, value: result, writable: true, configurable: true, enumerable: false};
            var fakePropertyDescriptor = new WI.PropertyDescriptor(fakeDescriptor, null, true, false, false, false);
            callback(fakePropertyDescriptor);
        }

        // FIXME: Implement a real RuntimeAgent.getOwnPropertyDescriptor?
        this.callFunction(backendGetOwnPropertyDescriptor, [propertyName], false, wrappedCallback.bind(this));
    }

    release()
    {
        if (this._objectId && !this._isFakeObject())
            this._target.RuntimeAgent.releaseObject(this._objectId);
    }

    arrayLength()
    {
        if (this._subtype !== "array")
            return 0;

        var matches = this._description.match(/\[([0-9]+)\]/);
        if (!matches)
            return 0;

        return parseInt(matches[1], 10);
    }

    asCallArgument()
    {
        return WI.RemoteObject.createCallArgument(this);
    }

    findFunctionSourceCodeLocation()
    {
        var result = new WI.WrappedPromise;

        if (!this._isFunction() || !this._objectId) {
            result.resolve(WI.RemoteObject.SourceCodeLocationPromise.MissingObjectId);
            return result.promise;
        }

        this._target.DebuggerAgent.getFunctionDetails(this._objectId, (error, response) => {
            if (error) {
                result.reject(error);
                return;
            }

            var location = response.location;
            var sourceCode = WI.debuggerManager.scriptForIdentifier(location.scriptId, this._target);

            if (!sourceCode || (!WI.isDebugUIEnabled() && isWebKitInternalScript(sourceCode.sourceURL))) {
                result.resolve(WI.RemoteObject.SourceCodeLocationPromise.NoSourceFound);
                return;
            }

            var sourceCodeLocation = sourceCode.createSourceCodeLocation(location.lineNumber, location.columnNumber || 0);
            result.resolve(sourceCodeLocation);
        });

        return result.promise;
    }

    // Private

    _isFakeObject()
    {
        return this._objectId === WI.RemoteObject.FakeRemoteObjectId;
    }

    _isSymbol()
    {
        return this._type === "symbol";
    }

    _isFunction()
    {
        return this._type === "function";
    }

    _weakCollectionObjectGroup()
    {
        return JSON.stringify(this._objectId) + "-" + this._subtype;
    }

    getPropertyDescriptorsAsObject(callback, options = {})
    {
        this.getPropertyDescriptors(function(properties) {
            var propertiesResult = {};
            var internalPropertiesResult = {};
            for (var propertyDescriptor of properties) {
                var object = propertyDescriptor.isInternalProperty ? internalPropertiesResult : propertiesResult;
                object[propertyDescriptor.name] = propertyDescriptor;
            }
            callback(propertiesResult, internalPropertiesResult);
        }, options);
    }

    _getPropertyDescriptorsResolver(callback, error, properties, internalProperties)
    {
        if (error) {
            callback(null);
            return;
        }

        let descriptors = properties.map((payload) => {
            return WI.PropertyDescriptor.fromPayload(payload, false, this._target);
        });

        if (internalProperties) {
            descriptors = descriptors.concat(internalProperties.map((payload) => {
                return WI.PropertyDescriptor.fromPayload(payload, true, this._target);
            }));
        }

        callback(descriptors);
    }

    // FIXME: Phase out these deprecated functions. They return DeprecatedRemoteObjectProperty instead of PropertyDescriptors.
    _deprecatedGetProperties(ownProperties, callback)
    {
        if (!this._objectId || this._isSymbol() || this._isFakeObject()) {
            callback([]);
            return;
        }

        this._target.RuntimeAgent.getProperties(this._objectId, ownProperties, this._deprecatedGetPropertiesResolver.bind(this, callback));
    }

    _deprecatedGetPropertiesResolver(callback, error, properties, internalProperties)
    {
        if (error) {
            callback(null);
            return;
        }

        if (internalProperties) {
            properties = properties.concat(internalProperties.map(function(descriptor) {
                descriptor.writable = false;
                descriptor.configurable = false;
                descriptor.enumerable = false;
                descriptor.isOwn = true;
                return descriptor;
            }));
        }

        var result = [];
        for (var i = 0; properties && i < properties.length; ++i) {
            var property = properties[i];
            if (property.get || property.set) {
                if (property.get)
                    result.push(new WI.DeprecatedRemoteObjectProperty("get " + property.name, WI.RemoteObject.fromPayload(property.get, this._target), property));
                if (property.set)
                    result.push(new WI.DeprecatedRemoteObjectProperty("set " + property.name, WI.RemoteObject.fromPayload(property.set, this._target), property));
            } else
                result.push(new WI.DeprecatedRemoteObjectProperty(property.name, WI.RemoteObject.fromPayload(property.value, this._target), property));
        }

        callback(result);
    }
};

WI.RemoteObject.FakeRemoteObjectId = "fake-remote-object";

WI.RemoteObject.SourceCodeLocationPromise = {
    NoSourceFound: "remote-object-source-code-location-promise-no-source-found",
    MissingObjectId: "remote-object-source-code-location-promise-missing-object-id"
};

// FIXME: Phase out this deprecated class.
WI.DeprecatedRemoteObjectProperty = class DeprecatedRemoteObjectProperty
{
    constructor(name, value, descriptor)
    {
        this.name = name;
        this.value = value;
        this.enumerable = descriptor ? !!descriptor.enumerable : true;
        this.writable = descriptor ? !!descriptor.writable : true;
        if (descriptor && descriptor.wasThrown)
            this.wasThrown = true;
    }

    // Static

    fromPrimitiveValue(name, value)
    {
        return new WI.DeprecatedRemoteObjectProperty(name, WI.RemoteObject.fromPrimitiveValue(value));
    }
};
