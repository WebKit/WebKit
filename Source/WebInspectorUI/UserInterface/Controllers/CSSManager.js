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

// FIXME: CSSManager lacks advanced multi-target support. (Stylesheets per-target)

WI.CSSManager = class CSSManager extends WI.Object
{
    constructor()
    {
        super();

        WI.Frame.addEventListener(WI.Frame.Event.MainResourceDidChange, this._mainResourceDidChange, this);
        WI.Frame.addEventListener(WI.Frame.Event.ResourceWasAdded, this._resourceAdded, this);
        WI.Resource.addEventListener(WI.SourceCode.Event.ContentDidChange, this._resourceContentDidChange, this);
        WI.Resource.addEventListener(WI.Resource.Event.TypeDidChange, this._resourceTypeDidChange, this);

        WI.DOMNode.addEventListener(WI.DOMNode.Event.AttributeModified, this._nodeAttributesDidChange, this);
        WI.DOMNode.addEventListener(WI.DOMNode.Event.AttributeRemoved, this._nodeAttributesDidChange, this);
        WI.DOMNode.addEventListener(WI.DOMNode.Event.EnabledPseudoClassesChanged, this._nodePseudoClassesDidChange, this);

        this._colorFormatSetting = new WI.Setting("default-color-format", WI.Color.Format.Original);

        this._styleSheetIdentifierMap = new Map;
        this._styleSheetFrameURLMap = new Map;
        this._nodeStylesMap = {};
        this._modifiedCSSRules = new Map;
        this._defaultAppearance = null;
        this._forcedAppearance = null;

        // COMPATIBILITY (iOS 9): Legacy backends did not send stylesheet
        // added/removed events and must be fetched manually.
        this._fetchedInitialStyleSheets = InspectorBackend.domains.CSS.hasEvent("styleSheetAdded");
    }

    // Target

    initializeTarget(target)
    {
        if (target.CSSAgent)
            target.CSSAgent.enable();
    }

    // Static

    static protocolStyleSheetOriginToEnum(origin)
    {
        switch (origin) {
        case CSSAgent.StyleSheetOrigin.Regular:
            return WI.CSSStyleSheet.Type.Author;
        case CSSAgent.StyleSheetOrigin.User:
            return WI.CSSStyleSheet.Type.User;
        case CSSAgent.StyleSheetOrigin.UserAgent:
            return WI.CSSStyleSheet.Type.UserAgent;
        case CSSAgent.StyleSheetOrigin.Inspector:
            return WI.CSSStyleSheet.Type.Inspector;
        default:
            console.assert(false, "Unknown CSS.StyleSheetOrigin", origin);
            return CSSAgent.StyleSheetOrigin.Regular;
        }
    }

    static protocolMediaSourceToEnum(source)
    {
        switch (source) {
        case CSSAgent.CSSMediaSource.MediaRule:
            return WI.CSSMedia.Type.MediaRule;
        case CSSAgent.CSSMediaSource.ImportRule:
            return WI.CSSMedia.Type.ImportRule;
        case CSSAgent.CSSMediaSource.LinkedSheet:
            return WI.CSSMedia.Type.LinkedStyleSheet;
        case CSSAgent.CSSMediaSource.InlineSheet:
            return WI.CSSMedia.Type.InlineStyleSheet;
        default:
            console.assert(false, "Unknown CSS.CSSMediaSource", source);
            return WI.CSSMedia.Type.MediaRule;
        }
    }

    // Public

    get preferredColorFormat()
    {
        return this._colorFormatSetting.value;
    }

    get styleSheets()
    {
        return [...this._styleSheetIdentifierMap.values()];
    }

    get defaultAppearance()
    {
        return this._defaultAppearance;
    }

    get forcedAppearance()
    {
        return this._forcedAppearance;
    }

    set forcedAppearance(name)
    {
        if (!this.canForceAppearance())
            return;

        let protocolName = "";

        switch (name) {
        case WI.CSSManager.Appearance.Light:
            protocolName = PageAgent.Appearance.Light;
            break;

        case WI.CSSManager.Appearance.Dark:
            protocolName = PageAgent.Appearance.Dark;
            break;

        case null:
        case undefined:
        case "":
            protocolName = "";
            break;

        default:
            // Abort for unknown values.
            return;
        }

        this._forcedAppearance = name || null;

        PageAgent.setForcedAppearance(protocolName).then(() => {
            this.mediaQueryResultChanged();
            this.dispatchEventToListeners(WI.CSSManager.Event.ForcedAppearanceDidChange, {appearance: this._forcedAppearance});
        });
    }

    canForceAppearance()
    {
        return window.PageAgent && !!PageAgent.setForcedAppearance && this._defaultAppearance;
    }

    canForcePseudoClasses()
    {
        return window.CSSAgent && !!CSSAgent.forcePseudoState;
    }

    propertyNameHasOtherVendorPrefix(name)
    {
        if (!name || name.length < 4 || name.charAt(0) !== "-")
            return false;

        var match = name.match(/^(?:-moz-|-ms-|-o-|-epub-)/);
        if (!match)
            return false;

        return true;
    }

    propertyValueHasOtherVendorKeyword(value)
    {
        var match = value.match(/(?:-moz-|-ms-|-o-|-epub-)[-\w]+/);
        if (!match)
            return false;

        return true;
    }

    canonicalNameForPropertyName(name)
    {
        if (!name || name.length < 8 || name.charAt(0) !== "-")
            return name;

        var match = name.match(/^(?:-webkit-|-khtml-|-apple-)(.+)/);
        if (!match)
            return name;

        return match[1];
    }

    fetchStyleSheetsIfNeeded()
    {
        if (this._fetchedInitialStyleSheets)
            return;

        this._fetchInfoForAllStyleSheets(function() {});
    }

    styleSheetForIdentifier(id)
    {
        let styleSheet = this._styleSheetIdentifierMap.get(id);
        if (styleSheet)
            return styleSheet;

        styleSheet = new WI.CSSStyleSheet(id);
        this._styleSheetIdentifierMap.set(id, styleSheet);
        return styleSheet;
    }

    stylesForNode(node)
    {
        if (node.id in this._nodeStylesMap)
            return this._nodeStylesMap[node.id];

        var styles = new WI.DOMNodeStyles(node);
        this._nodeStylesMap[node.id] = styles;
        return styles;
    }

    preferredInspectorStyleSheetForFrame(frame, callback, doNotCreateIfMissing)
    {
        var inspectorStyleSheets = this._inspectorStyleSheetsForFrame(frame);
        for (let styleSheet of inspectorStyleSheets) {
            if (styleSheet[WI.CSSManager.PreferredInspectorStyleSheetSymbol]) {
                callback(styleSheet);
                return;
            }
        }

        if (doNotCreateIfMissing)
            return;

        if (CSSAgent.createStyleSheet) {
            CSSAgent.createStyleSheet(frame.id, function(error, styleSheetId) {
                let styleSheet = WI.cssManager.styleSheetForIdentifier(styleSheetId);
                styleSheet[WI.CSSManager.PreferredInspectorStyleSheetSymbol] = true;
                callback(styleSheet);
            });
            return;
        }

        // COMPATIBILITY (iOS 9): CSS.createStyleSheet did not exist.
        // Legacy backends can only create the Inspector StyleSheet through CSS.addRule.
        // Exploit that to create the Inspector StyleSheet for the document.body node in
        // this frame, then get the StyleSheet for the new rule.

        let expression = appendWebInspectorSourceURL("document");
        let contextId = frame.pageExecutionContext.id;
        RuntimeAgent.evaluate.invoke({expression, objectGroup: "", includeCommandLineAPI: false, doNotPauseOnExceptionsAndMuteConsole: true, contextId, returnByValue: false, generatePreview: false}, documentAvailable);

        function documentAvailable(error, documentRemoteObjectPayload)
        {
            if (error) {
                callback(null);
                return;
            }

            let remoteObject = WI.RemoteObject.fromPayload(documentRemoteObjectPayload);
            remoteObject.pushNodeToFrontend(documentNodeAvailable.bind(null, remoteObject));
        }

        function documentNodeAvailable(remoteObject, documentNodeId)
        {
            remoteObject.release();

            if (!documentNodeId) {
                callback(null);
                return;
            }

            DOMAgent.querySelector(documentNodeId, "body", bodyNodeAvailable);
        }

        function bodyNodeAvailable(error, bodyNodeId)
        {
            if (error) {
                console.error(error);
                callback(null);
                return;
            }

            let selector = ""; // Intentionally empty.
            CSSAgent.addRule(bodyNodeId, selector, cssRuleAvailable);
        }

        function cssRuleAvailable(error, payload)
        {
            if (error || !payload.ruleId) {
                callback(null);
                return;
            }

            let styleSheetId = payload.ruleId.styleSheetId;
            let styleSheet = WI.cssManager.styleSheetForIdentifier(styleSheetId);
            if (!styleSheet) {
                callback(null);
                return;
            }

            styleSheet[WI.CSSManager.PreferredInspectorStyleSheetSymbol] = true;

            console.assert(styleSheet.isInspectorStyleSheet());
            console.assert(styleSheet.parentFrame === frame);

            callback(styleSheet);
        }
    }

    mediaTypeChanged()
    {
        // Act the same as if media queries changed.
        this.mediaQueryResultChanged();
    }

    defaultAppearanceDidChange(protocolName)
    {
        // Called from WI.PageObserver.

        let appearance = null;

        switch (protocolName) {
        case PageAgent.Appearance.Light:
            appearance = WI.CSSManager.Appearance.Light;
            break;

        case PageAgent.Appearance.Dark:
            appearance = WI.CSSManager.Appearance.Dark;
            break;

        default:
            console.error("Unknown default appearance name:", protocolName);
            break;
        }

        this._defaultAppearance = appearance;

        this.mediaQueryResultChanged();

        this.dispatchEventToListeners(WI.CSSManager.Event.DefaultAppearanceDidChange, {appearance});
    }

    get modifiedCSSRules()
    {
        return Array.from(this._modifiedCSSRules.values());
    }

    addModifiedCSSRule(cssRule)
    {
        this._modifiedCSSRules.set(cssRule.stringId, cssRule);
    }

    removeModifiedCSSRule(cssRule)
    {
        this._modifiedCSSRules.delete(cssRule.stringId);
    }

    // Protected

    mediaQueryResultChanged()
    {
        // Called from WI.CSSObserver.

        for (var key in this._nodeStylesMap)
            this._nodeStylesMap[key].mediaQueryResultDidChange();
    }

    styleSheetChanged(styleSheetIdentifier)
    {
        // Called from WI.CSSObserver.
        var styleSheet = this.styleSheetForIdentifier(styleSheetIdentifier);
        console.assert(styleSheet);

        // Do not observe inline styles
        if (styleSheet.isInlineStyleAttributeStyleSheet())
            return;

        styleSheet.noteContentDidChange();
        this._updateResourceContent(styleSheet);
    }

    styleSheetAdded(styleSheetInfo)
    {
        console.assert(!this._styleSheetIdentifierMap.has(styleSheetInfo.styleSheetId), "Attempted to add a CSSStyleSheet but identifier was already in use");
        let styleSheet = this.styleSheetForIdentifier(styleSheetInfo.styleSheetId);
        let parentFrame = WI.networkManager.frameForIdentifier(styleSheetInfo.frameId);
        let origin = WI.CSSManager.protocolStyleSheetOriginToEnum(styleSheetInfo.origin);
        styleSheet.updateInfo(styleSheetInfo.sourceURL, parentFrame, origin, styleSheetInfo.isInline, styleSheetInfo.startLine, styleSheetInfo.startColumn);

        this.dispatchEventToListeners(WI.CSSManager.Event.StyleSheetAdded, {styleSheet});
    }

    styleSheetRemoved(styleSheetIdentifier)
    {
        let styleSheet = this._styleSheetIdentifierMap.get(styleSheetIdentifier);
        console.assert(styleSheet, "Attempted to remove a CSSStyleSheet that was not tracked");
        if (!styleSheet)
            return;

        this._styleSheetIdentifierMap.delete(styleSheetIdentifier);

        this.dispatchEventToListeners(WI.CSSManager.Event.StyleSheetRemoved, {styleSheet});
    }

    // Private

    _inspectorStyleSheetsForFrame(frame)
    {
        let styleSheets = [];

        for (let styleSheet of this.styleSheets) {
            if (styleSheet.isInspectorStyleSheet() && styleSheet.parentFrame === frame)
                styleSheets.push(styleSheet);
        }

        return styleSheets;
    }

    _nodePseudoClassesDidChange(event)
    {
        var node = event.target;

        for (var key in this._nodeStylesMap) {
            var nodeStyles = this._nodeStylesMap[key];
            if (nodeStyles.node !== node && !nodeStyles.node.isDescendant(node))
                continue;
            nodeStyles.pseudoClassesDidChange(node);
        }
    }

    _nodeAttributesDidChange(event)
    {
        var node = event.target;

        for (var key in this._nodeStylesMap) {
            var nodeStyles = this._nodeStylesMap[key];
            if (nodeStyles.node !== node && !nodeStyles.node.isDescendant(node))
                continue;
            nodeStyles.attributeDidChange(node, event.data.name);
        }
    }

    _mainResourceDidChange(event)
    {
        console.assert(event.target instanceof WI.Frame);

        if (!event.target.isMainFrame())
            return;

        // Clear our maps when the main frame navigates.

        this._fetchedInitialStyleSheets = InspectorBackend.domains.CSS.hasEvent("styleSheetAdded");
        this._styleSheetIdentifierMap.clear();
        this._styleSheetFrameURLMap.clear();
        this._modifiedCSSRules.clear();

        this._nodeStylesMap = {};
    }

    _resourceAdded(event)
    {
        console.assert(event.target instanceof WI.Frame);

        var resource = event.data.resource;
        console.assert(resource);

        if (resource.type !== WI.Resource.Type.Stylesheet)
            return;

        this._clearStyleSheetsForResource(resource);
    }

    _resourceTypeDidChange(event)
    {
        console.assert(event.target instanceof WI.Resource);

        var resource = event.target;
        if (resource.type !== WI.Resource.Type.Stylesheet)
            return;

        this._clearStyleSheetsForResource(resource);
    }

    _clearStyleSheetsForResource(resource)
    {
        // Clear known stylesheets for this URL and frame. This will cause the stylesheets to
        // be updated next time _fetchInfoForAllStyleSheets is called.
        this._styleSheetIdentifierMap.delete(this._frameURLMapKey(resource.parentFrame, resource.url));
    }

    _frameURLMapKey(frame, url)
    {
        return frame.id + ":" + url;
    }

    _lookupStyleSheetForResource(resource, callback)
    {
        this._lookupStyleSheet(resource.parentFrame, resource.url, callback);
    }

    _lookupStyleSheet(frame, url, callback)
    {
        console.assert(frame instanceof WI.Frame);

        let key = this._frameURLMapKey(frame, url);

        function styleSheetsFetched()
        {
            callback(this._styleSheetFrameURLMap.get(key) || null);
        }

        let styleSheet = this._styleSheetFrameURLMap.get(key) || null;
        if (styleSheet)
            callback(styleSheet);
        else
            this._fetchInfoForAllStyleSheets(styleSheetsFetched.bind(this));
    }

    _fetchInfoForAllStyleSheets(callback)
    {
        console.assert(typeof callback === "function");

        function processStyleSheets(error, styleSheets)
        {
            this._styleSheetFrameURLMap.clear();

            if (error) {
                callback();
                return;
            }

            for (let styleSheetInfo of styleSheets) {
                let parentFrame = WI.networkManager.frameForIdentifier(styleSheetInfo.frameId);
                let origin = WI.CSSManager.protocolStyleSheetOriginToEnum(styleSheetInfo.origin);

                // COMPATIBILITY (iOS 9): The info did not have 'isInline', 'startLine', and 'startColumn', so make false and 0 in these cases.
                let isInline = styleSheetInfo.isInline || false;
                let startLine = styleSheetInfo.startLine || 0;
                let startColumn = styleSheetInfo.startColumn || 0;

                let styleSheet = this.styleSheetForIdentifier(styleSheetInfo.styleSheetId);
                styleSheet.updateInfo(styleSheetInfo.sourceURL, parentFrame, origin, isInline, startLine, startColumn);

                let key = this._frameURLMapKey(parentFrame, styleSheetInfo.sourceURL);
                this._styleSheetFrameURLMap.set(key, styleSheet);
            }

            callback();
        }

        CSSAgent.getAllStyleSheets(processStyleSheets.bind(this));
    }

    _resourceContentDidChange(event)
    {
        var resource = event.target;
        if (resource === this._ignoreResourceContentDidChangeEventForResource)
            return;

        // Ignore if it isn't a CSS stylesheet.
        if (resource.type !== WI.Resource.Type.Stylesheet || resource.syntheticMIMEType !== "text/css")
            return;

        function applyStyleSheetChanges()
        {
            function styleSheetFound(styleSheet)
            {
                resource.__pendingChangeTimeout = undefined;

                console.assert(styleSheet);
                if (!styleSheet)
                    return;

                // To prevent updating a TextEditor's content while the user is typing in it we want to
                // ignore the next _updateResourceContent call.
                resource.__ignoreNextUpdateResourceContent = true;

                WI.branchManager.currentBranch.revisionForRepresentedObject(styleSheet).content = resource.content;
            }

            this._lookupStyleSheetForResource(resource, styleSheetFound.bind(this));
        }

        if (resource.__pendingChangeTimeout)
            clearTimeout(resource.__pendingChangeTimeout);
        resource.__pendingChangeTimeout = setTimeout(applyStyleSheetChanges.bind(this), 500);
    }

    _updateResourceContent(styleSheet)
    {
        console.assert(styleSheet);

        function fetchedStyleSheetContent(parameters)
        {
            let representedObject = parameters.sourceCode;
            representedObject.__pendingChangeTimeout = undefined;

            console.assert(representedObject.url);
            if (!representedObject.url)
                return;

            if (!styleSheet.isInspectorStyleSheet()) {
                representedObject = representedObject.parentFrame.resourceForURL(representedObject.url);
                if (!representedObject)
                    return;

                // Only try to update stylesheet resources. Other resources, like documents, can contain
                // multiple stylesheets and we don't have the source ranges to update those.
                if (representedObject.type !== WI.Resource.Type.Stylesheet)
                    return;
            }

            if (representedObject.__ignoreNextUpdateResourceContent) {
                representedObject.__ignoreNextUpdateResourceContent = false;
                return;
            }

            this._ignoreResourceContentDidChangeEventForResource = representedObject;

            let revision = WI.branchManager.currentBranch.revisionForRepresentedObject(representedObject);
            if (styleSheet.isInspectorStyleSheet()) {
                revision.content = representedObject.content;
                styleSheet.dispatchEventToListeners(WI.SourceCode.Event.ContentDidChange);
            } else
                revision.content = parameters.content;

            this._ignoreResourceContentDidChangeEventForResource = null;
        }

        function styleSheetReady()
        {
            styleSheet.requestContent().then(fetchedStyleSheetContent.bind(this));
        }

        function applyStyleSheetChanges()
        {
            if (styleSheet.url)
                styleSheetReady.call(this);
            else
                this._fetchInfoForAllStyleSheets(styleSheetReady.bind(this));
        }

        if (styleSheet.__pendingChangeTimeout)
            clearTimeout(styleSheet.__pendingChangeTimeout);
        styleSheet.__pendingChangeTimeout = setTimeout(applyStyleSheetChanges.bind(this), 500);
    }
};

WI.CSSManager.Event = {
    StyleSheetAdded: "css-manager-style-sheet-added",
    StyleSheetRemoved: "css-manager-style-sheet-removed",
    DefaultAppearanceDidChange: "css-manager-default-appearance-did-change",
    ForcedAppearanceDidChange: "css-manager-forced-appearance-did-change",
};

WI.CSSManager.Appearance = {
    Light: Symbol("light"),
    Dark: Symbol("dark"),
};

WI.CSSManager.PseudoElementNames = ["before", "after"];
WI.CSSManager.ForceablePseudoClasses = ["active", "focus", "hover", "visited"];
WI.CSSManager.PreferredInspectorStyleSheetSymbol = Symbol("css-manager-preferred-inspector-stylesheet");
