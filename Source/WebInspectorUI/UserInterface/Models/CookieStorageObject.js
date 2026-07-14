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

WI.CookieStorageObject = class CookieStorageObject
{
    constructor(host)
    {
        this._host = host;
    }

    // Static

    static cookieMatchesResourceURL(cookie, resourceURL)
    {
        var parsedURL = parseURL(resourceURL);
        if (!parsedURL || !WI.CookieStorageObject.cookieDomainMatchesResourceDomain(cookie.domain, parsedURL.host))
            return false;

        return parsedURL.path.startsWith(cookie.path)
            && (!cookie.port || parsedURL.port === cookie.port)
            && (!cookie.secure || parsedURL.scheme === "https");
    }

    static cookieDomainMatchesResourceDomain(cookieDomain, resourceDomain)
    {
        if (cookieDomain.charAt(0) !== ".")
            return resourceDomain === cookieDomain;
        return !!resourceDomain.match(new RegExp("^(?:[^\\.]+\\.)*" + cookieDomain.substring(1).escapeForRegExp() + "$"), "i");
    }

    // Public

    get host()
    {
        return this._host;
    }

    // Under Site Isolation cookies are read through the "Storage" domain, which reads the authoritative
    // NetworkProcess store and so sees the out-of-process cross-origin iframe cookies the legacy in-process
    // Page cookie commands cannot. Otherwise the legacy Page path is used, leaving non-Site-Isolation
    // sessions unchanged. Callers use these accessors and methods and never touch a specific agent/target.

    get canGetCookies()
    {
        if (this._useStorageDomain)
            return WI.backendTarget.hasCommand("Storage.getCookies");
        return InspectorBackend.hasCommand("Page.getCookies");
    }

    get canSetCookie()
    {
        if (this._useStorageDomain)
            return WI.backendTarget.hasCommand("Storage.setCookie");
        return InspectorBackend.hasCommand("Page.setCookie");
    }

    get canDeleteCookie()
    {
        if (this._useStorageDomain)
            return WI.backendTarget.hasCommand("Storage.deleteCookies");
        return InspectorBackend.hasCommand("Page.deleteCookie");
    }

    // True when cookies can arrive from targets that appear after the initial load. Under Site Isolation
    // a cross-origin iframe becomes its own out-of-process target and its cookies then become readable,
    // so the view should refresh on TargetAdded. The legacy in-process path has no such late arrivals.
    get reloadsCookiesOnTargetAdded()
    {
        return this._useStorageDomain;
    }

    getCookies()
    {
        if (this._useStorageDomain) {
            // No filter: fetch the full authoritative store; the view buckets by host. Partition
            // "context" targets the inspected page's data store.
            return WI.backendTarget.StorageAgent.getCookies.invoke({partition: {type: "context"}}).then((payload) => payload.cookies);
        }

        return WI.assumingMainTarget().PageAgent.getCookies().then((payload) => payload.cookies);
    }

    setCookie(cookie, cookieProtocolPayload)
    {
        if (this._useStorageDomain)
            return WI.backendTarget.StorageAgent.setCookie.invoke({cookie: cookieProtocolPayload, partition: {type: "context"}});

        // COMPATIBILITY (macOS 15.2, iOS 18.2): `Page.setCookie` did not have a `shouldPartition` parameter yet.
        return WI.assumingMainTarget().PageAgent.setCookie.invoke({
            cookie: cookieProtocolPayload,
            shouldPartition: !cookie.partitionKey && !!cookie.partitioned,
        });
    }

    deleteCookie(cookie)
    {
        if (this._useStorageDomain) {
            // Storage.deleteCookies is filter-based; target this one cookie by its identity fields.
            let filter = {name: cookie.name};
            if (cookie.domain)
                filter.domain = cookie.domain;
            if (cookie.path)
                filter.path = cookie.path;
            return WI.backendTarget.StorageAgent.deleteCookies.invoke({filter, partition: {type: "context"}});
        }

        return WI.assumingMainTarget().PageAgent.deleteCookie(cookie.name, cookie.url);
    }

    saveIdentityToCookie(cookie)
    {
        // FIXME <https://webkit.org/b/151413>: This class should actually store cookie data for this host.
        cookie[WI.CookieStorageObject.CookieHostCookieKey] = this.host;
    }

    // Private

    get _useStorageDomain()
    {
        return WI.storageManager.shouldUseStorageDomain;
    }
};

WI.CookieStorageObject.TypeIdentifier = "cookie-storage";
WI.CookieStorageObject.CookieHostCookieKey = "cookie-storage-host";
