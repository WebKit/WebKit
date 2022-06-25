/*
 * Copyright (C) 2013, 2015 Apple Inc. All rights reserved.
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

WI.ApplicationCacheManifestTreeElement = class ApplicationCacheManifestTreeElement extends WI.StorageTreeElement
{
    constructor(representedObject)
    {
        console.assert(representedObject instanceof WI.ApplicationCacheManifest);

        super("application-cache-manifest", "", representedObject);

        this.hasChildren = true;
        this.expanded = true;
    }

    // Public

    get name()
    {
        if (!this._name)
            this._generateTitles();

        return this._name;
    }

    get secondaryName()
    {
        if (!this._secondaryName)
            this._generateTitles();

        return this._secondaryName;
    }

    get categoryName()
    {
        return WI.UIString("Application Cache");
    }

    _generateTitles()
    {
        var parsedURL = parseURL(this.representedObject.manifestURL);

        // Prefer the last path component, with a fallback for the host as the main title.
        this._name = WI.displayNameForURL(this.representedObject.manifestURL, parsedURL);

        // Show the host as the subtitle.
        var secondaryName = WI.displayNameForHost(parsedURL.host);
        this._secondaryName = this._name !== secondaryName ? secondaryName : null;
    }
};
