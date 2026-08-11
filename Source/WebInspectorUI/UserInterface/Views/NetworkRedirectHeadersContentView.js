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

WI.NetworkRedirectHeadersContentView = class NetworkRedirectHeadersContentView extends WI.ContentView
{
    constructor(redirect, parentResource, redirectIndex, parentEntry, delegate)
    {
        console.assert(redirect instanceof WI.Redirect, redirect);
        console.assert(parentResource instanceof WI.Resource, parentResource);

        const representedObject = null;
        super(representedObject);

        this._redirect = redirect;
        this._parentResource = parentResource;
        this._redirectIndex = redirectIndex;
        this._parentEntry = parentEntry;
        this._delegate = delegate;

        this.element.classList.add("resource-details", "resource-headers");
        this.element.tabIndex = 0;
    }

    // Protected

    initialLayout()
    {
        super.initialLayout();

        let summarySection = new WI.ResourceDetailsSection(WI.UIString("Summary", "Summary @ Network Redirect Headers", "Section header for network redirect details"), "summary");
        this.element.appendChild(summarySection.element);

        summarySection.toggleError(this._redirect.responseStatusCode >= 400);
        summarySection.appendKeyValuePair(WI.UIString("URL"), this._redirect.url.insertWordBreakCharacters(), "url");

        let status = emDash;
        if (this._redirect.responseStatusCode)
            status = `${this._redirect.responseStatusCode}${this._redirect.responseStatusText ? " " + this._redirect.responseStatusText : ""}`;
        summarySection.appendKeyValuePair(WI.UIString("Status"), status);

        summarySection.appendKeyValuePair(WI.UIString("Source"), WI.UIString("Network"));

        let requestSection = new WI.ResourceDetailsSection(WI.UIString("Request"), "redirect");
        this.element.appendChild(requestSection.element);
        requestSection.toggleError(this._redirect.responseStatusCode >= 400);

        // FIXME: <https://webkit.org/b/190214> Web Inspector: expose full load metrics for redirect requests
        requestSection.appendKeyValuePair(`${this._redirect.requestMethod} ${this._redirect.urlComponents.path}`, null, "h1-status");

        for (let [key, value] of this._createSortedArrayForHeaders(this._redirect.requestHeaders))
            requestSection.appendKeyValuePair(key, value, "header");

        let responseSection = new WI.ResourceDetailsSection(WI.UIString("Redirect Response"), "redirect");
        this.element.appendChild(responseSection.element);
        responseSection.toggleError(this._redirect.responseStatusCode >= 400);

        if (this._parentEntry?.redirectEntries) {
            let nextRedirectEntry = this._parentEntry.redirectEntries[this._redirectIndex + 1];

            let canNavigate = nextRedirectEntry
                ? typeof this._delegate?.networkRedirectHeadersContentViewShowRedirect === "function"
                : (typeof this._delegate?.showEntry === "function" || typeof this._delegate?.networkRedirectHeadersContentViewShowParentResource === "function");

            if (canNavigate) {
                let navLink = WI.createGoToArrowButton();
                navLink.classList.add("redirect-nav-link");

                if (nextRedirectEntry) {
                    navLink.title = WI.UIString("View Redirect");
                    navLink.addEventListener("click", (event) => {
                        event.preventDefault();
                        event.stopPropagation();
                        this._delegate.networkRedirectHeadersContentViewShowRedirect(this, nextRedirectEntry.redirect, this._parentResource, this._parentEntry);
                    });
                } else {
                    navLink.title = WI.UIString("View Response");
                    navLink.addEventListener("click", (event) => {
                        event.preventDefault();
                        event.stopPropagation();
                        if (this._delegate.showEntry)
                            this._delegate.showEntry(this._parentEntry);
                        else
                            this._delegate.networkRedirectHeadersContentViewShowParentResource(this, this._parentResource);
                    });
                }

                responseSection._titleElement.appendChild(navLink);
            }
        }

        responseSection.appendKeyValuePair(`${this._redirect.responseStatusCode} ${this._redirect.responseStatusText}`, null, "h1-status");

        for (let [key, value] of this._createSortedArrayForHeaders(this._redirect.responseHeaders))
            responseSection.appendKeyValuePair(key, value, "header");
    }

    // Private

    _createSortedArrayForHeaders(headers)
    {
        return Object.entries(headers).sort((a, b) => a[0].toLowerCase().extendedLocaleCompare(b[0].toLowerCase()));
    }
};
