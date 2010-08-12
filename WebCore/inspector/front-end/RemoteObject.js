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

WebInspector.RemoteObjectId = function(injectedScriptId, type, value, path)
{
    this.injectedScriptId = injectedScriptId;
    this.type = type;
    this.value = value;
    this.path = path || [];
}

WebInspector.RemoteObject = function(objectId, type, description, hasChildren)
{
    this._objectId = objectId;
    this._type = type;
    this._description = description;
    this._hasChildren = hasChildren;
}

WebInspector.RemoteObject.fromPrimitiveValue = function(value)
{
    return new WebInspector.RemoteObject(null, typeof value, value);
}

WebInspector.RemoteObject.fromNode = function(node, path)
{
    var objectId = new WebInspector.RemoteObjectId(-node.id, "node", node.id, path);
    return new WebInspector.RemoteObject(objectId, "node", appropriateSelectorForNode(node), true);
}

WebInspector.RemoteObject.fromPayload = function(payload)
{
    if (typeof payload === "object")
        return new WebInspector.RemoteObject(payload.objectId, payload.type, payload.description, payload.hasChildren);
    // FIXME: make sure we only get here with real payloads in the new DebuggerAgent.js.
    return payload;
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

    get description()
    {
        return this._description;
    },

    get hasChildren()
    {
        return this._hasChildren;
    },

    isError: function()
    {
        return this._type === "error";
    },

    getPropertyValueDescriptions: function(propertiesToQueryFor, callback)
    {
        function createPropertiesMapThenCallback(propertiesPayload)
        {
            if (!propertiesPayload) {
                callback();
                return;
            }
    
            var result = {};
            for (var i = 0; i < propertiesPayload.length; ++i)
                if (propertiesToQueryFor.indexOf(propertiesPayload[i].name) !== -1)
                    result[propertiesPayload[i].name] = propertiesPayload[i].value.description;
            callback(result);
        }
        this.getProperties(true, false, createPropertiesMapThenCallback);
    },

    getOwnProperties: function(abbreviate, callback)
    {
        this.getProperties(false, abbreviate, callback);
    },

    getProperties: function(ignoreHasOwnProperty, abbreviate, callback)
    {
        if (!this._objectId) {
            callback([]);
            return;
        }
        function remoteObjectBinder(properties)
        {
            for (var i = 0; properties && i < properties.length; ++i)
                properties[i].value = WebInspector.RemoteObject.fromPayload(properties[i].value);
            callback(properties);
        }
        InjectedScriptAccess.get(this._objectId.injectedScriptId).getProperties(this._objectId, ignoreHasOwnProperty, abbreviate, remoteObjectBinder);
    },

    setPropertyValue: function(name, value, callback)
    {
        if (!this._objectId) {
            callback(false);
            return;
        }
        InjectedScriptAccess.get(this._objectId.injectedScriptId).setPropertyValue(this._objectId, name, value, callback);
    }
}

WebInspector.RemoteObjectProperty = function(name, value)
{
    this.name = name;
    this.value = value;
}
