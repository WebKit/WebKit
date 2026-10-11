/*
 * Copyright (C) 2016 Devin Rousso <webkit@devinrousso.com>. All rights reserved.
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

WI.addMouseDownContextMenuHandlers = function(element, populateContextMenuCallback)
{
    let ignoreMouseDown = false;
    element.addEventListener("mousedown", (event) => {
        if (event.button !== 0)
            return;

        event.stop();

        if (ignoreMouseDown)
            return;

        let contextMenu = WI.ContextMenu.createFromEvent(event);
        contextMenu.addBeforeShowCallback(() => {
            ignoreMouseDown = false;
        });

        populateContextMenuCallback(contextMenu, event);

        ignoreMouseDown = !contextMenu.isEmpty();
        contextMenu.show();
    });

    element.addEventListener("contextmenu", (event) => {
        let contextMenu = WI.ContextMenu.createFromEvent(event);
        populateContextMenuCallback(contextMenu, event);
    });
};

WI.appendContextMenuItemsForNetworkResource = function(contextMenu, sourceCodeOrLocation)
{
    console.assert(contextMenu instanceof WI.ContextMenu, contextMenu);
    if (!(contextMenu instanceof WI.ContextMenu))
        return;

    let sourceCode = sourceCodeOrLocation;
    let displaySourceCode = sourceCode;
    let location = null;
    if (sourceCodeOrLocation instanceof WI.SourceCodeLocation) {
        sourceCode = sourceCodeOrLocation.sourceCode;
        displaySourceCode = sourceCodeOrLocation.displaySourceCode;
        location = sourceCodeOrLocation;
    }

    console.assert(sourceCode instanceof WI.SourceCode || sourceCode instanceof WI.Redirect, sourceCode);
    if (!(sourceCode instanceof WI.SourceCode) && !(sourceCode instanceof WI.Redirect))
        return;

    if (contextMenu.__domBreakpointItemsAdded)
        return;

    if (!contextMenu.__localOverrideItemsAdded && WI.NetworkManager.supportsOverridingResponses()) {
        contextMenu.__localOverrideItemsAdded = true;

        if (WI.networkManager.canBeOverridden(sourceCode)) {
            contextMenu.appendSeparator();

            if (WI.NetworkManager.supportsOverridingRequests()) {
                contextMenu.appendItem(WI.UIString("Create Request Local Override"), async () => {
                    let localResourceOverride = await sourceCode.createLocalResourceOverride(WI.LocalResourceOverride.InterceptType.Request);
                    WI.networkManager.addLocalResourceOverride(localResourceOverride);
                    WI.showLocalResourceOverride(localResourceOverride, {
                        overriddenResource: sourceCode,
                        initiatorHint: WI.TabBrowser.TabNavigationInitiator.ContextMenu,
                    });
                });
            }

            contextMenu.appendItem(WI.UIString("Create Response Local Override"), async () => {
                let localResourceOverride = await sourceCode.createLocalResourceOverride(WI.LocalResourceOverride.InterceptType.Response);
                WI.networkManager.addLocalResourceOverride(localResourceOverride);
                WI.showLocalResourceOverride(localResourceOverride, {
                    overriddenResource: sourceCode,
                    initiatorHint: WI.TabBrowser.TabNavigationInitiator.ContextMenu,
                });
            });

            if (WI.NetworkManager.supportsBlockingRequests()) {
                contextMenu.appendItem(WI.UIString("Block Request URL"), async () => {
                    let localResourceOverride = await sourceCode.createLocalResourceOverride(WI.LocalResourceOverride.InterceptType.Block);
                    WI.networkManager.addLocalResourceOverride(localResourceOverride);
                });
            }
        } else {
            let localResourceOverride = WI.networkManager.localResourceOverridesForURL(sourceCode.url)[0];
            if (localResourceOverride) {
                contextMenu.appendSeparator();

                contextMenu.appendItem(WI.UIString("Reveal Local Override"), () => {
                    WI.showLocalResourceOverride(localResourceOverride, {
                        overriddenResource: sourceCode,
                        initiatorHint: WI.TabBrowser.TabNavigationInitiator.ContextMenu,
                    });
                });

                contextMenu.appendItem(localResourceOverride.disabled ? WI.UIString("Enable Local Override") : WI.UIString("Disable Local Override"), () => {
                    localResourceOverride.disabled = !localResourceOverride.disabled;
                });

                contextMenu.appendItem(WI.UIString("Delete Local Override"), () => {
                    WI.networkManager.removeLocalResourceOverride(localResourceOverride);
                });
            }
        }
    }

    contextMenu.appendSeparator();

    if (location && (displaySourceCode instanceof WI.Script || (displaySourceCode instanceof WI.Resource && displaySourceCode.type === WI.Resource.Type.Script && !displaySourceCode.localResourceOverride))) {
        let existingJavaScriptBreakpoint = WI.debuggerManager.breakpointForSourceCodeLocation(location);
        if (existingJavaScriptBreakpoint) {
            contextMenu.appendItem(WI.UIString("Delete JavaScript Breakpoint"), () => {
                WI.debuggerManager.removeBreakpoint(existingJavaScriptBreakpoint);
            });
        } else {
            contextMenu.appendItem(WI.UIString("Add JavaScript Breakpoint"), () => {
                WI.debuggerManager.addBreakpoint(new WI.JavaScriptBreakpoint(location));
            });
        }
    }

    if (sourceCode?.initiatorStackTrace) {
        let existingURLBreakpoints = WI.domDebuggerManager.urlBreakpointsMatchingURL(sourceCode.url);
        if (existingURLBreakpoints.length) {
            contextMenu.appendItem(existingURLBreakpoints.length === 1 ? WI.UIString("Delete URL Breakpoint") : WI.UIString("Delete URL Breakpoints"), () => {
                for (let urlBreakpoint of existingURLBreakpoints)
                    WI.domDebuggerManager.removeURLBreakpoint(urlBreakpoint);
            });
        } else {
            contextMenu.appendItem(WI.UIString("Add URL Breakpoint"), () => {
                WI.domDebuggerManager.addURLBreakpoint(new WI.URLBreakpoint(WI.URLBreakpoint.Type.Text, sourceCode.url));
            });
        }
    }

    if (displaySourceCode.supportsScriptBlackboxing) {
        let blackboxData = WI.debuggerManager.blackboxDataForSourceCode(displaySourceCode);
        if (blackboxData && blackboxData.type === WI.DebuggerManager.BlackboxType.Pattern) {
            contextMenu.appendItem(WI.UIString("Reveal Blackbox Pattern"), () => {
                WI.showSettingsTab({
                    blackboxPatternToSelect: blackboxData.regex,
                    initiatorHint: WI.TabBrowser.TabNavigationInitiator.ContextMenu,
                });
            });
        } else {
            contextMenu.appendItem(blackboxData ? WI.UIString("Unblackbox Script") : WI.UIString("Blackbox Script"), () => {
                WI.debuggerManager.setShouldBlackboxScript(displaySourceCode, !blackboxData);
            });
        }
    }

    contextMenu.appendSeparator();

    WI.appendContextMenuItemsForURL(contextMenu, sourceCode.url, {sourceCode, location});

    let shouldShowCopyOptions = false;
    if (sourceCode instanceof WI.Resource && !sourceCode.localResourceOverride && sourceCode.hasMetadata)
        shouldShowCopyOptions = true;
    else if (sourceCode instanceof WI.Redirect)
        shouldShowCopyOptions = true;

    if (shouldShowCopyOptions && sourceCode.urlComponents.scheme !== "data") {
        contextMenu.appendItem(WI.UIString("Copy as fetch", "Copy the URL, method, headers, etc. of the given network request in the format of a JS fetch expression."), () => {
            InspectorFrontendHost.copyText(sourceCode.generateFetchCode());
        });

        contextMenu.appendItem(WI.UIString("Copy as cURL"), () => {
            InspectorFrontendHost.copyText(sourceCode.generateCURLCommand());
        });

        contextMenu.appendSeparator();

        contextMenu.appendItem(WI.UIString("Copy HTTP Request Headers"), () => {
            InspectorFrontendHost.copyText(sourceCode.stringifyHTTPRequestHeaders());
        });

        let hasResponseHeaders = sourceCode instanceof WI.Redirect || (sourceCode instanceof WI.Resource && sourceCode.hasResponse());
        if (hasResponseHeaders) {
            contextMenu.appendItem(WI.UIString("Copy HTTP Response Headers"), () => {
                InspectorFrontendHost.copyText(sourceCode.stringifyHTTPResponseHeaders());
            });
        }
    }

    if (WI.FileUtilities.canSave(WI.FileUtilities.SaveMode.SingleFile)) {
        contextMenu.appendSeparator();

        contextMenu.appendItem(WI.UIString("Save File"), () => {
            displaySourceCode.requestContent().then(() => {
                let saveData = {
                    url: displaySourceCode.url,
                    content: displaySourceCode.content,
                    base64Encoded: displaySourceCode.base64Encoded,
                };

                if (displaySourceCode.urlComponents.path === "/") {
                    let extension = WI.fileExtensionForMIMEType(displaySourceCode.mimeType);
                    if (extension)
                        saveData.suggestedName = `index.${extension}`;
                }

                WI.FileUtilities.save(WI.FileUtilities.SaveMode.SingleFile, saveData);
            });
        });
    }

    contextMenu.appendSeparator();
};

WI.appendContextMenuItemsForURL = function(contextMenu, url, options = {})
{
    if (!url)
        return;

    function showResourceWithOptions(options) {
        options.initiatorHint = WI.TabBrowser.TabNavigationInitiator.ContextMenu;
        if (options.location)
            WI.showSourceCodeLocation(options.location, options);
        else if (options.sourceCode)
            WI.showSourceCode(options.sourceCode, options);
        else
            WI.openURL(url, options);
    }

    if (!url.startsWith("javascript:") && !url.startsWith("data:")) {
        contextMenu.appendItem(WI.UIString("Open in New Window", "Open in New Window @ Context Menu Item", "Context menu item for opening the target item in a new window."), () => {
            WI.openURL(url, {alwaysOpenExternally: true});
        });
    }

    if (WI.networkManager.resourcesForURL(url).size) {
        if (!WI.isShowingSourcesTab()) {
            contextMenu.appendItem(WI.UIString("Reveal in Sources Tab"), () => {
                showResourceWithOptions({preferredTabType: WI.SourcesTabContentView.Type});
            });
        }

        if (!WI.isShowingNetworkTab()) {
            contextMenu.appendItem(WI.UIString("Reveal in Network Tab"), () => {
                showResourceWithOptions({preferredTabType: WI.NetworkTabContentView.Type});
            });
        }
    }

    contextMenu.appendSeparator();

    contextMenu.appendItem(WI.UIString("Copy Link"), () => {
        InspectorFrontendHost.copyText(url);
    });
};

WI.appendContextMenuItemsForDOMNode = function(contextMenu, domNodes, options = {})
{
    if (!Array.isArray(domNodes))
        domNodes = [domNodes];
    console.assert(domNodes.every((domNode) => domNode instanceof WI.DOMNode), domNodes);

    let multiple = domNodes.length > 1;
    let allAttached = domNodes.every((domNode) => domNode.attached);
    let allElements = allAttached && domNodes.every((domNode) => domNode.nodeType() === Node.ELEMENT_NODE);
    let allNonPseudo = allAttached && domNodes.every((domNode) => !domNode.isPseudoElement());

    let copySubMenu = options.copySubMenu || contextMenu.appendSubMenuItem(WI.UIString("Copy"));

    if (allElements) {
        copySubMenu.appendItem(WI.UIString("Selector Path"), () => {
            InspectorFrontendHost.copyText(domNodes.map((domNode) => WI.cssPath(domNode)).join("\n"));
        });
    }

    if (allNonPseudo) {
        copySubMenu.appendItem(WI.UIString("XPath"), () => {
            InspectorFrontendHost.copyText(domNodes.map((domNode) => WI.xpath(domNode)).join("\n"));
        });
    }

    contextMenu.appendSeparator();

    if (!options.usingLocalDOMNode) {
        if (!multiple && domNodes[0].isCustomElement()) {
            contextMenu.appendItem(WI.UIString("Jump to Definition"), () => {
                function didGetFunctionDetails(error, response) {
                    if (error)
                        return;

                    let location = response.location;
                    let sourceCode = WI.debuggerManager.scriptForIdentifier(location.scriptId, WI.mainTarget);
                    if (!sourceCode)
                        return;

                    let sourceCodeLocation = sourceCode.createSourceCodeLocation(location.lineNumber, location.columnNumber || 0);
                    WI.showSourceCodeLocation(sourceCodeLocation, {
                        ignoreNetworkTab: true,
                        ignoreSearchTab: true,
                    });
                }

                WI.RemoteObject.resolveNode(domNodes[0]).then((remoteObject) => {
                    remoteObject.getProperty("constructor", (error, result, wasThrown) => {
                        if (error)
                            return;
                        if (result.type === "function")
                            remoteObject.target.DebuggerAgent.getFunctionDetails(result.objectId, didGetFunctionDetails);
                        result.release();
                    });
                    remoteObject.release();
                });
            });

            contextMenu.appendSeparator();
        }

        if (!options.disallowEditing && allAttached && WI.cssManager.canForcePseudoClass()) {
            contextMenu.appendSeparator();

            let pseudoSubMenu = contextMenu.appendSubMenuItem(WI.UIString("Forced Pseudo-Classes", "A context menu item to force (override) a DOM node's pseudo-classes"));

            for (let pseudoClass of Object.values(WI.CSSManager.ForceablePseudoClass)) {
                if (!WI.cssManager.canForcePseudoClass(pseudoClass))
                    continue;

                let enabled = domNodes.every((domNode) => domNode.enabledPseudoClasses.includes(pseudoClass));
                pseudoSubMenu.appendCheckboxItem(WI.CSSManager.displayNameForForceablePseudoClass(pseudoClass), () => {
                    for (let domNode of domNodes)
                        domNode.setPseudoClassEnabled(pseudoClass, !enabled);
                }, enabled);
            }
        }

        if (allElements && allNonPseudo && WI.domDebuggerManager.supported) {
            contextMenu.appendSeparator();

            if (!multiple)
                WI.appendContextMenuItemsForDOMNodeBreakpoints(contextMenu, domNodes, options);
            else {
                let breakOnSubMenu = contextMenu.appendSubMenuItem(WI.UIString("Break on"));

                for (let type of Object.values(WI.DOMBreakpoint.Type)) {
                    let nodeBreakpoints = domNodes.map((domNode) => WI.domDebuggerManager.domBreakpointsForNode(domNode).find((domBreakpoint) => domBreakpoint.type === type));
                    let allExist = nodeBreakpoints.every((domBreakpoint) => !!domBreakpoint);

                    breakOnSubMenu.appendCheckboxItem(WI.DOMBreakpoint.displayNameForType(type), () => {
                        domNodes.forEach((domNode, index) => {
                            let domBreakpoint = nodeBreakpoints[index];
                            if (allExist)
                                WI.domDebuggerManager.removeDOMBreakpoint(domBreakpoint);
                            else if (!domBreakpoint)
                                WI.domDebuggerManager.addDOMBreakpoint(new WI.DOMBreakpoint(domNode, type));
                        });
                    }, allExist);
                }
            }
        }

        contextMenu.appendSeparator();

        if (!options.excludeLogElement && domNodes.every((domNode) => !domNode.destroyed && !domNode.isPseudoElement() && (!domNode.isInUserAgentShadowTree() || WI.DOMManager.supportsEditingUserAgentShadowTrees({frontendOnly: true})))) {
            let label;
            if (multiple)
                label = allElements ? WI.UIString("Log Elements", "Log (print) multiple DOM elements to Console") : WI.UIString("Log Nodes", "Log (print) multiple DOM nodes to Console");
            else
                label = allElements ? WI.UIString("Log Element", "Log (print) DOM element to Console") : WI.UIString("Log Node", "Log (print) DOM node to Console");
            contextMenu.appendItem(label, () => {
                for (let domNode of domNodes) {
                    WI.RemoteObject.resolveNode(domNode, WI.RuntimeManager.ConsoleObjectGroup).then((remoteObject) => {
                        let text = domNode.nodeType() === Node.ELEMENT_NODE ? WI.UIString("Selected Element", "Selected DOM element") : WI.UIString("Selected Node", "Selected DOM node");
                        WI.consoleLogViewController.appendImmediateExecutionWithResult(text, remoteObject, {addSpecialUserLogClass: true, shouldRevealConsole: true});
                    })
                }
            });
        }

        if (!options.excludeRevealElement && !multiple && allAttached && InspectorBackend.hasDomain("DOM")) {
            contextMenu.appendItem(WI.repeatedUIString.revealInDOMTree(), () => {
                WI.domManager.inspectElement(domNodes[0].id, {
                    initiatorHint: WI.TabBrowser.TabNavigationInitiator.ContextMenu,
                });
            });
        }

        if (!multiple && allAttached && InspectorBackend.hasDomain("LayerTree")) {
            contextMenu.appendItem(WI.UIString("Reveal in Layers Tab", "Open Layers tab and select the layer corresponding to this node"), () => {
                WI.showLayersTab({
                    nodeToSelect: domNodes[0],
                    initiatorHint: WI.TabBrowser.TabNavigationInitiator.ContextMenu,
                });
            });
        }

        if (allAttached && WI.FileUtilities.canSave(multiple ? WI.FileUtilities.SaveMode.MultipleFiles : WI.FileUtilities.SaveMode.SingleFile) && InspectorBackend.hasCommand("Page.snapshotNode")) {
            let label = multiple ? WI.UIString("Capture Screenshots", "Capture screenshots of the selected DOM nodes") : WI.UIString("Capture Screenshot", "Capture screenshot of the selected DOM node");
            contextMenu.appendItem(label, async () => {
                let target = WI.assumingMainTarget();

                let screenshotString = WI.FileUtilities.screenshotString();

                let saveDatas = [];
                for (let domNode of domNodes) {
                    let dataURL;
                    try {
                        ({dataURL} = await target.PageAgent.snapshotNode(domNode.id));
                    } catch (error) {
                        let consoleMessage = new WI.ConsoleMessage(target, WI.ConsoleMessage.MessageSource.Other, WI.ConsoleMessage.MessageLevel.Error, error.message);
                        consoleMessage.shouldRevealConsole = true;
                        WI.consoleLogViewController.appendConsoleMessage(consoleMessage);
                        continue;
                    }

                    let suggestedName = screenshotString;
                    if (multiple)
                        suggestedName += ` (${saveDatas.length + 1})`;
                    suggestedName += ".png";

                    saveDatas.push({
                        content: parseDataURL(dataURL).data,
                        base64Encoded: true,
                        suggestedName,
                    });
                }

                if (!saveDatas.length)
                    return;

                if (multiple)
                    WI.FileUtilities.save(WI.FileUtilities.SaveMode.MultipleFiles, saveDatas, true);
                else
                    WI.FileUtilities.save(WI.FileUtilities.SaveMode.SingleFile, saveDatas[0]);
            });
        }

        if (!multiple && allElements) {
            contextMenu.appendItem(WI.UIString("Scroll into View", "Scroll selected DOM node into view on the inspected web page"), () => {
                domNodes[0].scrollIntoView();
            });
        }

        contextMenu.appendSeparator();
    }
};

WI.appendContextMenuItemsForDOMNodeBreakpoints = function(contextMenu, domNodes, options = {})
{
    if (contextMenu.__domBreakpointItemsAdded)
        return;

    contextMenu.__domBreakpointItemsAdded = true;

    if (!Array.isArray(domNodes))
        domNodes = [domNodes];
    console.assert(domNodes.every((domNode) => domNode instanceof WI.DOMNode), domNodes);

    let breakpoints = domNodes.flatMap((domNode) => WI.domDebuggerManager.domBreakpointsForNode(domNode));

    contextMenu.appendSeparator();

    let subMenu = contextMenu.appendSubMenuItem(WI.UIString("Break on"));

    for (let type of Object.values(WI.DOMBreakpoint.Type)) {
        let label = WI.DOMBreakpoint.displayNameForType(type);
        let breakpointsWithType = breakpoints.filter((breakpoint) => breakpoint.type === type);

        subMenu.appendCheckboxItem(label, function() {
            if (breakpointsWithType.length > 0) {
                for (let breakpoint of breakpointsWithType)
                    WI.domDebuggerManager.removeDOMBreakpoint(breakpoint);
            } else {
                for (let domNode of domNodes)
                    WI.domDebuggerManager.addDOMBreakpoint(new WI.DOMBreakpoint(domNode, type));
            }
        }, breakpointsWithType.length);
    }

    if ((breakpoints.length === 1 || domNodes.length === 1) && !WI.isShowingSourcesTab()) {
        contextMenu.appendItem(breakpoints.length === 1 ? WI.UIString("Reveal Breakpoint in Sources Tab") : WI.UIString("Reveal Breakpoints in Sources Tab"), () => {
            WI.showSourcesTab({
                representedObjectToSelect: breakpoints.length === 1 ? breakpoints[0] : domNodes[0],
            });
        });
    }

    contextMenu.appendSeparator();

    if (breakpoints.length === 1)
        WI.BreakpointPopover.appendContextMenuItems(contextMenu, breakpoints[0], options.popoverTargetElement);
    else if (breakpoints.length) {
        let shouldEnable = breakpoints.some((breakpoint) => breakpoint.disabled);
        contextMenu.appendItem(shouldEnable ? WI.UIString("Enable Breakpoints") : WI.UIString("Disable Breakpoints"), () => {
            for (let breakpoint of breakpoints)
                breakpoint.disabled = !shouldEnable;
        });

        contextMenu.appendItem(WI.UIString("Delete Breakpoints"), () => {
            for (let breakpoint of breakpoints)
                WI.domDebuggerManager.removeDOMBreakpoint(breakpoint);
        });

        contextMenu.appendSeparator();
    }

    let subtreeBreakpoints = Array.from(new Set(domNodes.flatMap((domNode) => WI.domDebuggerManager.domBreakpointsInSubtree(domNode))));
    if (subtreeBreakpoints.length) {
        if (options.revealDescendantBreakpointsMenuItemHandler)
            contextMenu.appendItem(WI.UIString("Reveal Descendant Breakpoints"), options.revealDescendantBreakpointsMenuItemHandler);

        let subtreeShouldEnable = subtreeBreakpoints.some((breakpoint) => breakpoint.disabled);
        contextMenu.appendItem(subtreeShouldEnable ? WI.UIString("Enable Descendant Breakpoints") : WI.UIString("Disable Descendant Breakpoints"), () => {
            for (let subtreeBreakpoint of subtreeBreakpoints)
                subtreeBreakpoint.disabled = !subtreeShouldEnable;
        });

        contextMenu.appendItem(WI.UIString("Delete Descendant Breakpoints"), () => {
            for (let subtreeBreakpoint of subtreeBreakpoints)
                WI.domDebuggerManager.removeDOMBreakpoint(subtreeBreakpoint);
        });

        contextMenu.appendSeparator();
    }
};
