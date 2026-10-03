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

WI.SourceCode = class SourceCode extends WI.Object
{
    constructor(url)
    {
        super();

        this._url = url;
        this._urlComponents = null;

        this._originalRevision = new WI.SourceCodeRevision(this);
        this._currentRevision = this._originalRevision;

        this._sourceMaps = null;
        this._formatterSourceMap = null;
        this._requestContentPromise = null;
    }

    // Static

    static generateSpecialContentForURL(url)
    {
        if (url === "about:blank") {
            return Promise.resolve({
                content: "",
                message: WI.unlocalizedString("about:blank")
            });
        }
        return null;
    }

    static clearDisplayNameAffixes()
    {
        WI.SourceCode._sourceCodesForDisplayName.clear();
        WI.SourceCode._urlDisplayNameForSourceCode = new WeakMap;
        WI.SourceCode._affixesForSourceCode.clear();
    }

    static _regenerateAffixesForDisplayName(displayName)
    {
        let sourceCodes = Array.from(WI.SourceCode._sourceCodesForDisplayName.get(displayName) || []);

        let affixesForSourceCode = new Map;
        if (sourceCodes.length < 2) {
            for (let sourceCode of sourceCodes)
                affixesForSourceCode.set(sourceCode, {prefix: "", suffix: ""});
        } else {
            let candidates = sourceCodes.map((sourceCode) => {
                let urlComponents = sourceCode.urlComponents;
                let path = urlComponents.path || "";
                if (path.endsWith("/"))
                    path = path.substring(0, path.length - 1);
                let lastSlashIndex = path.lastIndexOf("/");
                let directory = lastSlashIndex > 0 ? path.substring(0, lastSlashIndex) : "";
                let prefixSegments = directory.split("/").filter((segment) => segment).map(tryDecodeURIComponent).reverse();

                let origin = urlComponents.host ? urlComponents.host + (urlComponents.port ? ":" + urlComponents.port : "") : "";
                if (origin)
                    prefixSegments.push(origin);

                return {
                    sourceCode,
                    url: sourceCode.url,
                    displayName: WI.displayNameForURL(sourceCode.url, urlComponents),
                    prefixSegments,
                    shownSegmentCount: 0,
                    suffix: urlComponents.queryString ? "?" + tryDecodeURIComponent(urlComponents.queryString) : "",
                };
            });

            function generatePrefix(candidate) {
                if (!candidate.shownSegmentCount)
                    return "";
                return candidate.prefixSegments.slice(0, candidate.shownSegmentCount).reverse().join("/") + "/";
            }

            function generateSuffix(candidate) {
                return candidate.suffix;
            }

            while (true) {
                let candidatesForDisplayName = new Multimap;
                for (let candidate of candidates) {
                    let displayName = generatePrefix(candidate) + candidate.displayName + generateSuffix(candidate);
                    candidatesForDisplayName.add(displayName, candidate);
                }

                let didGrow = false;
                for (let [, candidatesWithDisplayName] of candidatesForDisplayName.sets()) {
                    if (candidatesWithDisplayName.size < 2)
                        continue;

                    let duplicate = true;
                    for (let candidate of candidatesWithDisplayName) {
                        if (candidate.url !== candidatesWithDisplayName.firstValue.url) {
                            duplicate = false;
                            break;
                        }
                    }
                    if (duplicate)
                        continue;

                    for (let candidate of candidatesWithDisplayName) {
                        if (candidate.shownSegmentCount < candidate.prefixSegments.length) {
                            ++candidate.shownSegmentCount;
                            didGrow = true;
                        }
                    }
                }
                if (!didGrow)
                    break;
            }

            for (let candidate of candidates) {
                affixesForSourceCode.set(candidate.sourceCode, {
                    prefix: generatePrefix(candidate),
                    suffix: generateSuffix(candidate),
                });
            }
        }

        let changedSourceCodes = [];
        for (let sourceCode of sourceCodes) {
            let {prefix, suffix} = affixesForSourceCode.get(sourceCode) || {};
            let {prefix: previousPrefix, suffix: previousSuffix} = WI.SourceCode._affixesForSourceCode.get(sourceCode) || {};

            if (!prefix && !suffix) {
                if (previousPrefix || previousSuffix) {
                    WI.SourceCode._affixesForSourceCode.delete(sourceCode);
                    changedSourceCodes.push(sourceCode);
                }
                continue;
            }

            if (previousPrefix !== prefix || previousSuffix !== suffix) {
                WI.SourceCode._affixesForSourceCode.set(sourceCode, {prefix, suffix});
                changedSourceCodes.push(sourceCode);
            }
        }

        for (let sourceCode of changedSourceCodes)
            sourceCode.dispatchEventToListeners(WI.SourceCode.Event.DisplayNameChanged);
    }

    // Public

    get displayName()
    {
        // Implemented by subclasses.
        console.error("Needs to be implemented by a subclass.");
        return "";
    }

    displayNameWithAffix(options = {})
    {
        const displayName = WI.displayNameForURL(this._url, this.urlComponents, options);

        this._regenerateAffixesForDisplayName(displayName);

        let affix = WI.SourceCode._affixesForSourceCode.get(this);
        if (!affix)
            return displayName;

        return affix.prefix + displayName + affix.suffix;
    }

    get originalRevision()
    {
        return this._originalRevision;
    }

    get currentRevision()
    {
        return this._currentRevision;
    }

    set currentRevision(revision)
    {
        console.assert(revision instanceof WI.SourceCodeRevision);
        if (!(revision instanceof WI.SourceCodeRevision))
            return;

        console.assert(revision.sourceCode === this);
        if (revision.sourceCode !== this)
            return;

        this._currentRevision = revision;

        this.dispatchEventToListeners(WI.SourceCode.Event.ContentDidChange);
    }

    get editableRevision()
    {
        if (this._currentRevision === this._originalRevision)
            this._currentRevision = this._originalRevision.copy();
        return this._currentRevision;
    }

    get content()
    {
        return this._currentRevision.content;
    }

    get base64Encoded()
    {
        return this._currentRevision.base64Encoded;
    }

    get url()
    {
        return this._url;
    }

    get urlComponents()
    {
        if (!this._urlComponents)
            this._urlComponents = parseURL(this._url);
        return this._urlComponents;
    }

    get contentIdentifier()
    {
        // A contentIdentifier is roughly `url || sourceURL` for cases where
        // the content is consistent between sessions and not ephemeral.

        // Can be overridden by subclasses if better behavior is possible.
        return this.url;
    }

    get isScript()
    {
        // Implemented by subclasses if needed.
        return false;
    }

    get supportsScriptBlackboxing()
    {
        if (!this.isScript)
            return false;
        if (!WI.DebuggerManager.supportsBlackboxingScripts())
            return false;
        let contentIdentifier = this.contentIdentifier;
        return contentIdentifier && !isWebKitInjectedScript(contentIdentifier);
    }

    get localResourceOverride()
    {
        // Overridden by subclasses if needed.
        return null;
    }

    get sourceMaps()
    {
        return this._sourceMaps || [];
    }

    addSourceMap(sourceMap)
    {
        console.assert(sourceMap instanceof WI.SourceMap);

        if (!this._sourceMaps)
            this._sourceMaps = [];

        this._sourceMaps.push(sourceMap);

        this.dispatchEventToListeners(WI.SourceCode.Event.SourceMapAdded, {sourceMap});
    }

    get formatterSourceMap()
    {
        return this._formatterSourceMap;
    }

    set formatterSourceMap(formatterSourceMap)
    {
        console.assert(this._formatterSourceMap === null || formatterSourceMap === null);
        console.assert(formatterSourceMap === null || formatterSourceMap instanceof WI.FormatterSourceMap);

        this._formatterSourceMap = formatterSourceMap;

        this.dispatchEventToListeners(WI.SourceCode.Event.FormatterDidChange);
    }

    requestContent()
    {
        this._requestContentPromise = this._requestContentPromise || this.requestContentFromBackend().then(this._processContent.bind(this));

        return this._requestContentPromise;
    }

    createSourceCodeLocation(lineNumber, columnNumber)
    {
        return new WI.SourceCodeLocation(this, lineNumber, columnNumber);
    }

    createLazySourceCodeLocation(lineNumber, columnNumber)
    {
        return new WI.LazySourceCodeLocation(this, lineNumber, columnNumber);
    }

    createSourceCodeTextRange(textRange)
    {
        return new WI.SourceCodeTextRange(this, textRange);
    }

    createSourceMapSourceCodeLocation(lineNumber, columnNumber)
    {
        // Overridden by subclasses if needed.
        return this.createSourceCodeLocation(lineNumber, columnNumber);
    }

    createSourceMapPosition(lineNumber, columnNumber)
    {
        // Overridden by subclasses if needed.
        return new WI.SourceCodePosition(lineNumber, columnNumber);
    }

    // Protected

    revisionContentDidChange(revision)
    {
        if (this._ignoreRevisionContentDidChangeEvent)
            return;

        console.assert(revision === this._currentRevision);
        if (revision !== this._currentRevision)
            return;

        this.handleCurrentRevisionContentChange();

        this.dispatchEventToListeners(WI.SourceCode.Event.ContentDidChange);
    }

    handleCurrentRevisionContentChange()
    {
        // Implemented by subclasses if needed.
    }

    get revisionForRequestedContent()
    {
        // Implemented by subclasses if needed.
        return this._originalRevision;
    }

    markContentAsStale()
    {
        this._requestContentPromise = null;
        this._contentReceived = false;
    }

    requestContentFromBackend()
    {
        // Implemented by subclasses.
        console.error("Needs to be implemented by a subclass.");
        return Promise.reject(new Error("Needs to be implemented by a subclass."));
    }

    get mimeType()
    {
        // Implemented by subclasses.
        console.error("Needs to be implemented by a subclass.");
        return null;
    }

    // Private

    _regenerateAffixesForDisplayName(displayName)
    {
        let {url: previousURL, displayName: previousDisplayName} = WI.SourceCode._urlDisplayNameForSourceCode.get(this) || {};
        if (previousURL === this._url)
            return;

        if (previousDisplayName) {
            WI.SourceCode._sourceCodesForDisplayName.get(previousDisplayName)?.delete(this);
            WI.SourceCode._affixesForSourceCode.delete(this);
        }

        if (!this.urlComponents.lastPathComponent || isWebKitInternalScript(this._url) || this._url.startsWith("data:")) {
            WI.SourceCode._urlDisplayNameForSourceCode.delete(this);
            if (previousDisplayName)
                WI.SourceCode._regenerateAffixesForDisplayName(previousDisplayName);
            return;
        }

        WI.SourceCode._urlDisplayNameForSourceCode.set(this, {url: this._url, displayName});
        WI.SourceCode._sourceCodesForDisplayName.getOrInsert(displayName, new IterableWeakSet).add(this);

        if (previousDisplayName && previousDisplayName !== displayName)
            WI.SourceCode._regenerateAffixesForDisplayName(previousDisplayName);

        WI.SourceCode._regenerateAffixesForDisplayName(displayName);
    }

    _processContent(parameters)
    {
        // Different backend APIs return one of `content, `body`, `text`, or `scriptSource`.
        let rawContent = parameters.content || parameters.body || parameters.text || parameters.scriptSource;
        let rawBase64Encoded = !!parameters.base64Encoded;
        let content = rawContent;
        let error = parameters.error;
        let message = parameters.message;

        if (parameters.base64Encoded)
            content = content ? WI.BlobUtilities.decodeBase64ToBlob(content, this.mimeType) : "";

        let revision = this.revisionForRequestedContent;

        this._ignoreRevisionContentDidChangeEvent = true;
        revision.updateRevisionContent(rawContent, {
            base64Encoded: rawBase64Encoded,
            mimeType: this.mimeType,
            blobContent: content instanceof Blob ? content : null,
        });
        this._ignoreRevisionContentDidChangeEvent = false;

        // FIXME: Returning the content in this promise is misleading. It may not be current content
        // now, and it may become out-dated later on. We should drop content from this promise
        // and require clients to ask for the current contents from the sourceCode in the result.
        // That would also avoid confusion around `content` being a Blob and eliminate the work
        // of creating the Blob if it is not used.

        return Promise.resolve({
            error,
            message,
            sourceCode: this,
            content,
            rawContent,
            rawBase64Encoded,
        });
    }
};

WI.SourceCode.Event = {
    ContentDidChange: "source-code-content-did-change",
    SourceMapAdded: "source-code-source-map-added",
    FormatterDidChange: "source-code-formatter-did-change",
    LoadingDidFinish: "source-code-loading-did-finish",
    LoadingDidFail: "source-code-loading-did-fail",
    DisplayNameChanged: "source-code-display-name-changed",
};

WI.SourceCode._sourceCodesForDisplayName = new Map;
WI.SourceCode._urlDisplayNameForSourceCode = new WeakMap;
WI.SourceCode._affixesForSourceCode = new IterableWeakMap;
