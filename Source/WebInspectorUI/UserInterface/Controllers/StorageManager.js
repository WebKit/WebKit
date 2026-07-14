/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
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

// Manages the "Storage" protocol domain (cookies for now). Storage is a "web-page" domain that,
// like "Browser", lives on the UIProcess (multiplexing) backend target -- it reads the authoritative
// NetworkProcess cookie store, so it sees cookies across every WebContent process, including the
// out-of-process cross-origin iframes created under Site Isolation.
//
// This manager owns only the enable/disable lifecycle and exposes shouldUseStorageDomain. The actual
// cookie get/set/delete (and the choice of Storage-vs-Page path) lives on WI.CookieStorageObject, so
// views never issue cookie commands or know which backend serves them.

WI.StorageManager = class StorageManager extends WI.Object
{
    constructor()
    {
        super();

        this._enabled = false;
    }

    // Agent

    get domains() { return ["Storage"]; }

    activateExtraDomain(domain)
    {
        // COMPATIBILITY (iOS 14.0): Inspector.activateExtraDomains was removed in favor of a declared debuggable type

        console.assert(domain === "Storage");

        for (let target of WI.targets)
            this.initializeTarget(target);
    }

    // Target

    initializeTarget(target)
    {
        if (!this._enabled)
            return;

        // COMPATIBILITY (macOS 26.2, iOS 26.2): The Storage domain did not exist yet. Like Browser,
        // it lives on the UIProcess (multiplexing) target, exposed as WI.backendTarget.StorageAgent.
        if (target.hasDomain("Storage"))
            target.StorageAgent.enable();
    }

    // Public

    // The Storage domain is the correct cookie source only when Site Isolation is actually active:
    // that is when cross-origin iframes run out-of-process and the legacy in-process Page cookie
    // commands cannot see their cookies. In a non-Site-Isolation session the old Page path is left
    // unchanged. The domain must also be present on the backend target (older backends lack it).
    get shouldUseStorageDomain()
    {
        return WI.isSiteIsolationEnabled() && this.hasStorageDomain;
    }

    get hasStorageDomain()
    {
        return !!WI.backendTarget && WI.backendTarget.hasDomain("Storage");
    }

    enable()
    {
        console.assert(!this._enabled);

        this._enabled = true;

        for (let target of WI.targets)
            this.initializeTarget(target);
    }

    disable()
    {
        console.assert(this._enabled);

        for (let target of WI.targets) {
            // COMPATIBILITY (macOS 26.2, iOS 26.2): The Storage domain did not exist yet.
            if (target.hasDomain("Storage"))
                target.StorageAgent.disable();
        }

        this._enabled = false;
    }
};
