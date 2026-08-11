/*
 * Copyright (C) 2025 Devin Rousso <webkit@devinrousso.com>. All rights reserved.
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

WI.ImportedTarget = class ImportedTarget
{
    constructor(identifier, type, name, url)
    {
        this._identifier = identifier;
        this._type = type;
        this._name = name;
        this._url = url;
    }

    // Import / Export

    static import(json)
    {
        let {identifier, type, name, url} = json;

        for (let existing of WI.ImportedTarget._forIdentifierMap.values()) {
            if (existing._identifier === identifier && existing._type === type && existing._name === name && existing._url === url)
                return existing;
        }

        let target = new WI.ImportedTarget(identifier, type, name, url);

        WI.ImportedTarget._forIdentifierMap.set(target.identifier, target);

        return target;
    }

    exportData()
    {
        return {
            identifier: this._identifier,
            type: this._type,
            name: this._name,
            url: this._url,
        };
    }

    // Static

    static forIdentifier(targetId)
    {
        return WI.ImportedTarget._forIdentifierMap.get(targetId) || null;
    }

    // Public

    get identifier() { return this._identifier; }
    get type() { return this._type; }
    get name() { return this._name; }
    get url() { return this._url; }

    get isDestroyed() { return true; }

    get displayName() { return this._name; }

    hasDomain(domainName)
    {
        console.assert(false, "not reached");
        return false;
    }

    hasCommand(qualifiedName, parameterName)
    {
        console.assert(false, "not reached");
        return false;
    }

    hasEvent(qualifiedName, parameterName)
    {
        console.assert(false, "not reached");
        return false;
    }
};

WI.ImportedTarget._forIdentifierMap = new Map;
