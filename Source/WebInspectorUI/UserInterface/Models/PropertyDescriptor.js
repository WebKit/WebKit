/*
 * Copyright (C) 2015 Apple Inc. All rights reserved.
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

WebInspector.PropertyDescriptor = function(descriptor, isOwnProperty, wasThrown, nativeGetter, isInternalProperty)
{
    WebInspector.Object.call(this);

    console.assert(descriptor);
    console.assert(descriptor.name);
    console.assert(!descriptor.value || descriptor.value instanceof WebInspector.RemoteObject);
    console.assert(!descriptor.get || descriptor.get instanceof WebInspector.RemoteObject);
    console.assert(!descriptor.set || descriptor.set instanceof WebInspector.RemoteObject);

    this._name = descriptor.name;
    this._value = descriptor.value;
    this._hasValue = "value" in descriptor;
    this._get = descriptor.get;
    this._set = descriptor.set;

    this._writable = descriptor.writable || false;
    this._configurable = descriptor.configurable || false;
    this._enumerable = descriptor.enumerable || false;

    this._own = isOwnProperty || false;
    this._wasThrown = wasThrown || false;
    this._nativeGetterValue = nativeGetter || false;
    this._internal = isInternalProperty || false;
};

// Runtime.PropertyDescriptor or Runtime.InternalPropertyDescriptor (second argument).
WebInspector.PropertyDescriptor.fromPayload = function(payload, internal)
{
    if (payload.value)
        payload.value = WebInspector.RemoteObject.fromPayload(payload.value);
    if (payload.get)
        payload.get = WebInspector.RemoteObject.fromPayload(payload.get);
    if (payload.set)
        payload.set = WebInspector.RemoteObject.fromPayload(payload.set);

    if (internal) {
        console.assert(payload.value);
        payload.writable = payload.configurable = payload.enumerable = false;
        payload.isOwn = true;
    }

    return new WebInspector.PropertyDescriptor(payload, payload.isOwn, payload.wasThrown, payload.nativeGetter, internal);
};

WebInspector.PropertyDescriptor.prototype = {
    constructor: WebInspector.PropertyDescriptor,
    __proto__: WebInspector.Object.prototype,

    // Public

    get name()
    {
        return this._name;
    },

    get value()
    {
        return this._value;
    },

    get get()
    {
        return this._get;
    },

    get set()
    {
        return this._set;
    },

    get writable()
    {
        return this._writable;
    },

    get configurable()
    {
        return this._configurable;
    },

    get enumerable()
    {
        return this._enumerable;
    },

    get isOwnProperty()
    {
        return this._own;
    },

    get wasThrown()
    {
        return this._wasThrown;
    },

    get nativeGetter()
    {
        return this._nativeGetterValue;
    },

    get isInternalProperty()
    {
        return this._internal;
    },

    hasValue: function()
    {
        return this._hasValue;
    },

    hasGetter: function()
    {
        return this._get && this._get.type === "function";
    },

    hasSetter: function()
    {
        return this._set && this._set.type === "function";
    },

    isIndexProperty: function()
    {
        return !isNaN(Number(this._name));
    },
};
