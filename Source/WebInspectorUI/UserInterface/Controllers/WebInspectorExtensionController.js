/*
 * Copyright (C) 2020-2021 Apple Inc. All rights reserved.
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

WI.WebInspectorExtensionController = class WebInspectorExtensionController extends WI.Object
{
    constructor()
    {
        super();

        this._extensionForExtensionIDMap = new Map;
        this._extensionTabContentViewForExtensionTabIDMap = new Map;
        this._tabIDsForExtensionIDMap = new Multimap;
        this._nextExtensionTabID = 1;
    }

    // Public

    registerExtension(extensionID, displayName)
    {
        if (this._extensionForExtensionIDMap.has(extensionID)) {
            WI.reportInternalError("Unable to register extension, it's already registered: " + extensionID);
            return WI.WebInspectorExtension.ErrorCode.RegistrationFailed;
        }

        let extension = new WI.WebInspectorExtension(extensionID, displayName);
        this._extensionForExtensionIDMap.set(extensionID, extension);

        this.dispatchEventToListeners(WI.WebInspectorExtensionController.Event.ExtensionAdded, {extension});
    }

    unregisterExtension(extensionID)
    {
        let extension = this._extensionForExtensionIDMap.take(extensionID);
        if (!extension) {
            WI.reportInternalError("Unable to unregister extension with unknown ID: " + extensionID);
            return WI.WebInspectorExtension.ErrorCode.InvalidRequest;
        }

        let extensionTabIDsToRemove = this._tabIDsForExtensionIDMap.take(extensionID) || [];
        for (let extensionTabID of extensionTabIDsToRemove) {
            let tabContentView = this._extensionTabContentViewForExtensionTabIDMap.take(extensionTabID);
            WI.tabBrowser.closeTabForContentView(tabContentView);
        }

        this.dispatchEventToListeners(WI.WebInspectorExtensionController.Event.ExtensionRemoved, {extension});
    }

    createTabForExtension(extensionID, tabName, tabIconURL, sourceURL)
    {
        let extension = this._extensionForExtensionIDMap.get(extensionID);
        if (!extension) {
            WI.reportInternalError("Unable to create tab for extension with unknown ID: " + extensionID + " sourceURL: " + sourceURL);
            return WI.WebInspectorExtension.ErrorCode.InvalidRequest;
        }

        let extensionTabID = `WebExtensionTab-${extensionID}-${this._nextExtensionTabID++}`;
        let tabContentView = new WI.WebInspectorExtensionTabContentView(extension, extensionTabID, tabName, tabIconURL, sourceURL);

        this._tabIDsForExtensionIDMap.add(extensionID, extensionTabID);
        this._extensionTabContentViewForExtensionTabIDMap.set(extensionTabID, tabContentView);
        WI.tabBrowser.addTabForContentView(tabContentView);

        // The calling convention is to return an error string or a result object.
        return {extensionTabID};
    }

    evaluateScriptForExtension(extensionID, scriptSource, {frameURL, contextSecurityOrigin, useContentScriptContext} = {})
    {
        let extension = this._extensionForExtensionIDMap.get(extensionID);
        if (!extension) {
            WI.reportInternalError("Unable to evaluate script for extension with unknown ID: " + extensionID);
            return WI.WebInspectorExtension.ErrorCode.InvalidRequest;
        }

        // FIXME: <rdar://problem/74180355> implement execution context selection options
        if (frameURL) {
            WI.reportInternalError("evaluateScriptForExtension: the 'frameURL' option is not yet implemented.");
            return WI.WebInspectorExtension.ErrorCode.NotImplemented;
        }

        if (contextSecurityOrigin) {
            WI.reportInternalError("evaluateScriptForExtension: the 'contextSecurityOrigin' option is not yet implemented.");
            return WI.WebInspectorExtension.ErrorCode.NotImplemented;
        }

        if (useContentScriptContext) {
            WI.reportInternalError("evaluateScriptForExtension: the 'useContentScriptContext' option is not yet implemented.");
            return WI.WebInspectorExtension.ErrorCode.NotImplemented;
        }

        let evaluationContext = WI.runtimeManager.activeExecutionContext;
        return evaluationContext.target.RuntimeAgent.evaluate.invoke({
            expression: scriptSource,
            objectGroup: "extension-evaluation",
            includeCommandLineAPI: true,
            returnByValue: true,
            generatePreview: false,
            saveResult: false,
            contextId: evaluationContext.id,
        }).then((payload) => {
            let resultOrError = payload.result;
            let wasThrown = payload.wasThrown;
            let {type, value} = resultOrError;
            return wasThrown ? {"error": resultOrError.description} : {"result": value};
        }).catch((error) => error.description);
    }
    
    reloadForExtension(extensionID, {ignoreCache, userAgent, injectedScript} = {})
    {
        let extension = this._extensionForExtensionIDMap.get(extensionID);
        if (!extension) {
            WI.reportInternalError("Unable to evaluate script for extension with unknown ID: " + extensionID);
            return WI.WebInspectorExtension.ErrorCode.InvalidRequest;
        }

        // FIXME: <webkit.org/b/222328> Implement `userAgent` and `injectedScript` options for `devtools.inspectedWindow.reload` command
        if (userAgent) {
            WI.reportInternalError("reloadForExtension: the 'userAgent' option is not yet implemented.");
            return WI.WebInspectorExtension.ErrorCode.NotImplemented;
        }

        if (injectedScript) {
            WI.reportInternalError("reloadForExtension: the 'injectedScript' option is not yet implemented.");
            return WI.WebInspectorExtension.ErrorCode.NotImplemented;
        }
        
        let target = WI.assumingMainTarget();
        if (!target.hasCommand("Page.reload"))
            return WI.WebInspectorExtension.ErrorCode.InvalidRequest;
        
        return target.PageAgent.reload.invoke({ignoreCache});
        
        
    }
};

WI.WebInspectorExtensionController.Event = {
    ExtensionAdded: "extension-added",
    ExtensionRemoved: "extension-removed",
};
