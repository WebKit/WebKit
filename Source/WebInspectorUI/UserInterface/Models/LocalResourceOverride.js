/*
 * Copyright (C) 2019 Apple Inc. All rights reserved.
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

WI.LocalResourceOverride = class LocalResourceOverride extends WI.Object
{
    constructor(type, localResource, {isCaseSensitive, isRegex, disabled} = {})
    {
        console.assert(Object.values(WI.LocalResourceOverride.InterceptType).includes(type), type);
        console.assert(localResource instanceof WI.LocalResource, localResource);
        console.assert(localResource.isLocalResourceOverride, localResource);
        console.assert(localResource.url, localResource);
        console.assert(isCaseSensitive === undefined || typeof isCaseSensitive === "boolean", isCaseSensitive);
        console.assert(isRegex === undefined || typeof isRegex === "boolean", isRegex);
        console.assert(disabled === undefined || typeof disabled === "boolean", disabled);

        super();

        this._type = type;
        this._localResource = localResource;
        this._isCaseSensitive = isCaseSensitive !== undefined ? isCaseSensitive : true;
        this._isRegex = isRegex !== undefined ? isRegex : false;
        this._disabled = disabled !== undefined ? disabled : false;
    }

    // Static

    static create(type, {url, mimeType, content, base64Encoded, statusCode, statusText, headers, isCaseSensitive, isRegex, disabled})
    {
        let localResource = new WI.LocalResource({
            request: {
                url: isRegex ? url : WI.urlWithoutFragment(url),
            },
            response: {
                headers,
                mimeType,
                statusCode,
                statusText,
                content,
                base64Encoded,
            },
            isLocalResourceOverride: true,
        });

        return new WI.LocalResourceOverride(type, localResource, {isCaseSensitive, isRegex, disabled});
    }

    // Import / Export

    static fromJSON(json)
    {
        let {type, localResource, isCaseSensitive, isRegex, disabled} = json;

        // COMPATIBILITY (iOS 13.4): Network.interceptWithRequest/Network.interceptRequestWithResponse did not exist yet.
        type ??= WI.LocalResourceOverride.InterceptType.Response;

        return new WI.LocalResourceOverride(type, WI.LocalResource.fromJSON(localResource), {isCaseSensitive, isRegex, disabled});
    }

    toJSON(key)
    {
        let json = {
            type: this._type,
            localResource: this._localResource.toJSON(key),
            isCaseSensitive: this._isCaseSensitive,
            isRegex: this._isRegex,
            disabled: this._disabled,
        };

        if (key === WI.ObjectStore.toJSONSymbol)
            json[WI.objectStores.localResourceOverrides.keyPath] = this._localResource.url;

        return json;
    }

    // Public

    get type() { return this._type; }
    get url() { return this._localResource.url; }
    get localResource() { return this._localResource; }
    get isCaseSensitive() { return this._isCaseSensitive; }
    get isRegex() { return this._isRegex; }

    get disabled()
    {
        return this._disabled;
    }

    set disabled(disabled)
    {
        if (this._disabled === disabled)
            return;

        this._disabled = !!disabled;

        this.dispatchEventToListeners(WI.LocalResourceOverride.Event.DisabledChanged);
    }

    matches(url)
    {
        if (this._isRegex) {
            let regex = new RegExp(this.url, !this._isCaseSensitive ? "i" : "");
            return regex.test(url);
        }

        if (!this._isCaseSensitive)
            return String(url).toLowerCase() === this.url.toLowerCase();

        return url === this.url;
    }

    // Protected

    saveIdentityToCookie(cookie)
    {
        cookie["local-resource-override-type"] = this._type;
        cookie["local-resource-override-url"] = this._localResource.url;
        cookie["local-resource-override-is-case-sensitive"] = this._isCaseSensitive;
        cookie["local-resource-override-is-regex"] = this._isRegex;
        cookie["local-resource-override-disabled"] = this._disabled;
    }
};

WI.LocalResourceOverride.TypeIdentifier = "local-resource-override";

WI.LocalResourceOverride.InterceptType = {
    Response: "response",
    ResponseSkippingNetwork: "response-skipping-network",
};

WI.LocalResourceOverride.Event = {
    DisabledChanged: "local-resource-override-disabled-state-did-change",
};
