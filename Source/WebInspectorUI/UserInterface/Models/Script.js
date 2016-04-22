/*
 * Copyright (C) 2013 Apple Inc. All rights reserved.
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

WebInspector.Script = class Script extends WebInspector.SourceCode
{
    constructor(id, range, url, injected, sourceURL, sourceMapURL)
    {
        super();

        console.assert(id);
        console.assert(range instanceof WebInspector.TextRange);

        this._id = id || null;
        this._range = range || null;
        this._url = url || null;
        this._sourceURL = sourceURL || null;
        this._injected = injected || false;

        this._resource = this._resolveResource();
        if (this._resource)
            this._resource.associateWithScript(this);

        if (sourceMapURL)
            WebInspector.sourceMapManager.downloadSourceMap(sourceMapURL, this._url, this);

        this._scriptSyntaxTree = null;
    }

    // Static

    static resetUniqueDisplayNameNumbers()
    {
        WebInspector.Script._nextUniqueDisplayNameNumber = 1;
    }

    // Public

    get id()
    {
        return this._id;
    }

    get range()
    {
        return this._range;
    }

    get url()
    {
        return this._url;
    }

    get sourceURL()
    {
        return this._sourceURL;
    }

    get urlComponents()
    {
        if (!this._urlComponents)
            this._urlComponents = parseURL(this._url);
        return this._urlComponents;
    }

    get mimeType()
    {
        return this._resource.mimeType;
    }

    get displayName()
    {
        if (this._url)
            return WebInspector.displayNameForURL(this._url, this.urlComponents);

        if (this._sourceURL) {
            if (!this._sourceURLComponents)
                this._sourceURLComponents = parseURL(this._sourceURL);
            return WebInspector.displayNameForURL(this._sourceURL, this._sourceURLComponents);
        }

        // Assign a unique number to the script object so it will stay the same.
        if (!this._uniqueDisplayNameNumber)
            this._uniqueDisplayNameNumber = this.constructor._nextUniqueDisplayNameNumber++;

        return WebInspector.UIString("Anonymous Script %d").format(this._uniqueDisplayNameNumber);
    }

    get displayURL()
    {
        const isMultiLine = true;
        const dataURIMaxSize = 64;

        if (this._url)
            return WebInspector.truncateURL(this._url, isMultiLine, dataURIMaxSize);
        if (this._sourceURL)
            return WebInspector.truncateURL(this._sourceURL, isMultiLine, dataURIMaxSize);
        return null;
    }

    get injected()
    {
        return this._injected;
    }

    get anonymous()
    {
        return !this._resource && !this._url && !this._sourceURL;
    }

    get resource()
    {
        return this._resource;
    }

    get scriptSyntaxTree()
    {
        return this._scriptSyntaxTree;
    }

    requestContentFromBackend()
    {
        if (!this._id) {
            // There is no identifier to request content with. Return false to cause the
            // pending callbacks to get null content.
            return Promise.reject(new Error("There is no identifier to request content with."));
        }

        return DebuggerAgent.getScriptSource(this._id);
    }

    saveIdentityToCookie(cookie)
    {
        cookie[WebInspector.Script.URLCookieKey] = this.url;
        cookie[WebInspector.Script.DisplayNameCookieKey] = this.displayName;
    }

    requestScriptSyntaxTree(callback)
    {
        if (this._scriptSyntaxTree) {
            setTimeout(function() { callback(this._scriptSyntaxTree); }.bind(this), 0);
            return;
        }

        var makeSyntaxTreeAndCallCallback = function(content)
        {
            this._makeSyntaxTree(content);
            callback(this._scriptSyntaxTree);
        }.bind(this);

        var content = this.content;
        if (!content && this._resource && this._resource.type === WebInspector.Resource.Type.Script && this._resource.finished)
            content = this._resource.content;
        if (content) {
            setTimeout(makeSyntaxTreeAndCallCallback, 0, content);
            return;
        }

        this.requestContent().then(function(parameters) {
            makeSyntaxTreeAndCallCallback(parameters.sourceCode.content);
        }).catch(function(error) {
            makeSyntaxTreeAndCallCallback(null);
        });
    }

    // Private

    _resolveResource()
    {
        // FIXME: We should be able to associate a Script with a Resource through identifiers,
        // we shouldn't need to lookup by URL, which is not safe with frames, where there might
        // be multiple resources with the same URL.
        // <rdar://problem/13373951> Scripts should be able to associate directly with a Resource

        // No URL, no resource.
        if (!this._url)
            return null;

        try {
            // Try with the Script's full URL.
            var resource = WebInspector.frameResourceManager.resourceForURL(this.url);
            if (resource)
                return resource;

            // Try with the Script's full decoded URL.
            var decodedURL = decodeURI(this._url);
            if (decodedURL !== this._url) {
                resource = WebInspector.frameResourceManager.resourceForURL(decodedURL);
                if (resource)
                    return resource;
            }

            // Next try removing any fragment in the original URL.
            var urlWithoutFragment = removeURLFragment(this._url);
            if (urlWithoutFragment !== this._url) {
                resource = WebInspector.frameResourceManager.resourceForURL(urlWithoutFragment);
                if (resource)
                    return resource;
            }

            // Finally try removing any fragment in the decoded URL.
            var decodedURLWithoutFragment = removeURLFragment(decodedURL);
            if (decodedURLWithoutFragment !== decodedURL) {
                resource = WebInspector.frameResourceManager.resourceForURL(decodedURLWithoutFragment);
                if (resource)
                    return resource;
            }
        } catch (e) {
            // Ignore possible URIErrors.
        }

        return null;
    }

    _makeSyntaxTree(sourceText)
    {
        if (this._scriptSyntaxTree || !sourceText)
            return;

        this._scriptSyntaxTree = new WebInspector.ScriptSyntaxTree(sourceText, this);
    }
};

WebInspector.Script.TypeIdentifier = "script";
WebInspector.Script.URLCookieKey = "script-url";
WebInspector.Script.DisplayNameCookieKey = "script-display-name";

WebInspector.Script._nextUniqueDisplayNameNumber = 1;
