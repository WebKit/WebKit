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

WebInspector.SourceMapManager = class SourceMapManager extends WebInspector.Object
{
    constructor()
    {
        super();

        this._sourceMapURLMap = {};
        this._downloadingSourceMaps = {};

        WebInspector.Frame.addEventListener(WebInspector.Frame.Event.MainResourceDidChange, this._mainResourceDidChange, this);
    }

    // Public

    sourceMapForURL(sourceMapURL)
    {
        return this._sourceMapURLMap[sourceMapURL];
    }

    downloadSourceMap(sourceMapURL, baseURL, originalSourceCode)
    {
        sourceMapURL = absoluteURL(sourceMapURL, baseURL);
        if (!sourceMapURL)
            return;

        console.assert(originalSourceCode.url);
        if (!originalSourceCode.url)
            return;

        // FIXME: <rdar://problem/13265694> Source Maps: Better handle when multiple resources reference the same SourceMap

        if (sourceMapURL in this._sourceMapURLMap)
            return;

        if (sourceMapURL in this._downloadingSourceMaps)
            return;

        function loadAndParseSourceMap()
        {
            this._loadAndParseSourceMap(sourceMapURL, baseURL, originalSourceCode);
        }

        if (!WebInspector.frameResourceManager.mainFrame) {
            // If we don't have a main frame, then we are likely in the middle of building the resource tree.
            // Delaying until the next runloop is enough in this case to then start loading the source map.
            setTimeout(loadAndParseSourceMap.bind(this), 0);
            return;
        }

        loadAndParseSourceMap.call(this);
    }

    // Private

    _loadAndParseSourceMap(sourceMapURL, baseURL, originalSourceCode)
    {
        this._downloadingSourceMaps[sourceMapURL] = true;

        function sourceMapLoaded(error, content, mimeType, statusCode)
        {
            if (error || statusCode >= 400) {
                this._loadAndParseFailed(sourceMapURL);
                return;
            }

            if (content.slice(0, 3) === ")]}") {
                var firstNewlineIndex = content.indexOf("\n");
                if (firstNewlineIndex === -1) {
                    this._loadAndParseFailed(sourceMapURL);
                    return;
                }

                content = content.substring(firstNewlineIndex);
            }

            try {
                var payload = JSON.parse(content);
                var baseURL = sourceMapURL.startsWith("data:") ? originalSourceCode.url : sourceMapURL;
                var sourceMap = new WebInspector.SourceMap(baseURL, payload, originalSourceCode);
                this._loadAndParseSucceeded(sourceMapURL, sourceMap);
            } catch(e) {
                this._loadAndParseFailed(sourceMapURL);
            }
        }

        if (sourceMapURL.startsWith("data:")) {
            let {mimeType, base64, data} = parseDataURL(sourceMapURL);
            let content = base64 ? atob(data) : data;
            sourceMapLoaded.call(this, null, content, mimeType, 0);
            return;
        }

        // COMPATIBILITY (iOS 7): Network.loadResource did not exist.
        // Also, JavaScript Debuggable may reach this.
        if (!window.NetworkAgent || !NetworkAgent.loadResource) {
            this._loadAndParseFailed(sourceMapURL);
            return;
        }

        var frameIdentifier = null;
        if (originalSourceCode instanceof WebInspector.Resource && originalSourceCode.parentFrame)
            frameIdentifier = originalSourceCode.parentFrame.id;

        if (!frameIdentifier)
            frameIdentifier = WebInspector.frameResourceManager.mainFrame.id;

        NetworkAgent.loadResource(frameIdentifier, sourceMapURL, sourceMapLoaded.bind(this));
    }

    _loadAndParseFailed(sourceMapURL)
    {
        delete this._downloadingSourceMaps[sourceMapURL];
    }

    _loadAndParseSucceeded(sourceMapURL, sourceMap)
    {
        if (!(sourceMapURL in this._downloadingSourceMaps))
            return;

        delete this._downloadingSourceMaps[sourceMapURL];

        this._sourceMapURLMap[sourceMapURL] = sourceMap;

        var sources = sourceMap.sources();
        for (var i = 0; i < sources.length; ++i) {
            var sourceMapResource = new WebInspector.SourceMapResource(sources[i], sourceMap);
            sourceMap.addResource(sourceMapResource);
        }

        // Associate the SourceMap with the originalSourceCode.
        sourceMap.originalSourceCode.addSourceMap(sourceMap);

        // If the originalSourceCode was not a Resource, be sure to also associate with the Resource if one exists.
        // FIXME: We should try to use the right frame instead of a global lookup by URL.
        if (!(sourceMap.originalSourceCode instanceof WebInspector.Resource)) {
            console.assert(sourceMap.originalSourceCode instanceof WebInspector.Script);
            var resource = sourceMap.originalSourceCode.resource;
            if (resource)
                resource.addSourceMap(sourceMap);
        }
    }

    _mainResourceDidChange(event)
    {
        if (!event.target.isMainFrame())
            return;

        this._sourceMapURLMap = {};
        this._downloadingSourceMaps = {};
    }
};
