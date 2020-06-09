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

        ignoreMouseDown = true;

        let contextMenu = WI.ContextMenu.createFromEvent(event);
        contextMenu.addBeforeShowCallback(() => {
            ignoreMouseDown = false;
        });

        populateContextMenuCallback(contextMenu, event);

        contextMenu.show();
    });

    element.addEventListener("contextmenu", (event) => {
        let contextMenu = WI.ContextMenu.createFromEvent(event);
        populateContextMenuCallback(contextMenu, event);
    });
};

WI.appendContextMenuItemsForSourceCode = function(contextMenu, sourceCodeOrLocation)
{
    console.assert(contextMenu instanceof WI.ContextMenu);
    if (!(contextMenu instanceof WI.ContextMenu))
        return;

    let sourceCode = sourceCodeOrLocation;
    let location = null;
    if (sourceCodeOrLocation instanceof WI.SourceCodeLocation) {
        sourceCode = sourceCodeOrLocation.sourceCode;
        location = sourceCodeOrLocation;
    }

    console.assert(sourceCode instanceof WI.SourceCode);
    if (!(sourceCode instanceof WI.SourceCode))
        return;

    if (contextMenu.__domBreakpointItemsAdded)
        return;

    if (!contextMenu.__localOverrideItemsAdded && WI.NetworkManager.supportsLocalResourceOverrides()) {
        contextMenu.__localOverrideItemsAdded = true;

        if (WI.networkManager.canBeOverridden(sourceCode)) {
            contextMenu.appendSeparator();
            contextMenu.appendItem(WI.UIString("Create Local Override"), async () => {
                let resource = sourceCode;
                let localResourceOverride = await resource.createLocalResourceOverride();
                WI.networkManager.addLocalResourceOverride(localResourceOverride);
                WI.showLocalResourceOverride(localResourceOverride, {
                    initiatorHint: WI.TabBrowser.TabNavigationInitiator.ContextMenu,
                });
            });
        } else {
            let localResourceOverride = WI.networkManager.localResourceOverrideForURL(sourceCode.url);
            if (localResourceOverride) {
                contextMenu.appendSeparator();

                contextMenu.appendItem(WI.UIString("Reveal Local Override"), () => {
                    WI.showLocalResourceOverride(localResourceOverride, {
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

    if (location && (sourceCode instanceof WI.Script || (sourceCode instanceof WI.Resource && sourceCode.type === WI.Resource.Type.Script && !sourceCode.isLocalResourceOverride))) {
        let existingBreakpoint = WI.debuggerManager.breakpointForSourceCodeLocation(location);
        if (existingBreakpoint) {
            contextMenu.appendItem(WI.UIString("Delete Breakpoint"), () => {
                WI.debuggerManager.removeBreakpoint(existingBreakpoint);
            });
        } else {
            contextMenu.appendItem(WI.UIString("Add Breakpoint"), () => {
                WI.debuggerManager.addBreakpoint(new WI.Breakpoint(location));
            });
        }
    }

    if (sourceCode.supportsScriptBlackboxing) {
        let blackboxData = WI.debuggerManager.blackboxDataForSourceCode(sourceCode);
        if (blackboxData && blackboxData.type === WI.DebuggerManager.BlackboxType.Pattern) {
            contextMenu.appendItem(WI.UIString("Reveal Blackbox Pattern"), () => {
                WI.showSettingsTab({
                    blackboxPatternToSelect: blackboxData.regex,
                    initiatorHint: WI.TabBrowser.TabNavigationInitiator.ContextMenu,
                });
            });
        } else {
            contextMenu.appendItem(blackboxData ? WI.UIString("Unblackbox Script") : WI.UIString("Blackbox Script"), () => {
                WI.debuggerManager.setShouldBlackboxScript(sourceCode, !blackboxData);
            });
        }
    }

    contextMenu.appendSeparator();

    WI.appendContextMenuItemsForURL(contextMenu, sourceCode.url, {sourceCode, location});

    if (sourceCode instanceof WI.Resource && !sourceCode.isLocalResourceOverride) {
        if (sourceCode.urlComponents.scheme !== "data") {
            contextMenu.appendItem(WI.UIString("Copy as cURL"), () => {
                InspectorFrontendHost.copyText(sourceCode.generateCURLCommand());
            });

            contextMenu.appendSeparator();

            contextMenu.appendItem(WI.UIString("Copy HTTP Request"), () => {
                InspectorFrontendHost.copyText(sourceCode.stringifyHTTPRequest());
            });

            if (sourceCode.hasResponse()) {
                contextMenu.appendItem(WI.UIString("Copy HTTP Response"), () => {
                    InspectorFrontendHost.copyText(sourceCode.stringifyHTTPResponse());
                });
            }
        }
    }

    contextMenu.appendSeparator();

    contextMenu.appendItem(WI.UIString("Save File"), () => {
        sourceCode.requestContent().then(() => {
            let saveData = {
                url: sourceCode.url,
                content: sourceCode.content,
            };

            if (sourceCode.urlComponents.path === "/") {
                let extension = WI.fileExtensionForMIMEType(sourceCode.mimeType);
                if (extension)
                    saveData.suggestedName = `index.${extension}`;
            }

            const forceSaveAs = true;
            WI.FileUtilities.save(saveData, forceSaveAs);
        });
    });

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
            WI.openURL(url, options.frame, options);
    }

    if (!url.startsWith("javascript:") && !url.startsWith("data:")) {
        contextMenu.appendItem(WI.UIString("Open in New Tab"), () => {
            const frame = null;
            WI.openURL(url, frame, {alwaysOpenExternally: true});
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

WI.appendContextMenuItemsForDOMNode = function(contextMenu, domNode, options = {})
{
    console.assert(contextMenu instanceof WI.ContextMenu);
    if (!(contextMenu instanceof WI.ContextMenu))
        return;

    console.assert(domNode instanceof WI.DOMNode);
    if (!(domNode instanceof WI.DOMNode))
        return;

    let copySubMenu = options.copySubMenu || contextMenu.appendSubMenuItem(WI.UIString("Copy"));

    let isElement = domNode.nodeType() === Node.ELEMENT_NODE;
    let attached = domNode.attached;

    if (isElement && attached) {
        copySubMenu.appendItem(WI.UIString("Selector Path"), () => {
            let cssPath = WI.cssPath(domNode);
            InspectorFrontendHost.copyText(cssPath);
        });
    }

    if (!domNode.isPseudoElement() && attached) {
        copySubMenu.appendItem(WI.UIString("XPath"), () => {
            let xpath = WI.xpath(domNode);
            InspectorFrontendHost.copyText(xpath);
        });
    }

    contextMenu.appendSeparator();

    if (!options.usingLocalDOMNode) {
        if (domNode.isCustomElement()) {
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

                WI.RemoteObject.resolveNode(domNode).then((remoteObject) => {
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

        if (!options.disallowEditing && WI.cssManager.canForcePseudoClasses() && domNode.attached) {
            contextMenu.appendSeparator();

            let pseudoSubMenu = contextMenu.appendSubMenuItem(WI.UIString("Forced Pseudo-Classes", "A context menu item to force (override) a DOM node's pseudo-classes"));

            let enabledPseudoClasses = domNode.enabledPseudoClasses;
            WI.CSSManager.ForceablePseudoClasses.forEach((pseudoClass) => {
                let enabled = enabledPseudoClasses.includes(pseudoClass);
                pseudoSubMenu.appendCheckboxItem(pseudoClass.capitalize(), () => {
                    domNode.setPseudoClassEnabled(pseudoClass, !enabled);
                }, enabled);
            });
        }

        if (WI.domDebuggerManager.supported && isElement && !domNode.isPseudoElement() && attached) {
            contextMenu.appendSeparator();

            WI.appendContextMenuItemsForDOMNodeBreakpoints(contextMenu, domNode, options);
        }

        contextMenu.appendSeparator();

        let canLogShadowTree = !domNode.isInUserAgentShadowTree() || WI.DOMManager.supportsEditingUserAgentShadowTrees({frontendOnly: true});
        if (!options.excludeLogElement && canLogShadowTree && !domNode.destroyed && !domNode.isPseudoElement()) {
            let label = isElement ? WI.UIString("Log Element", "Log (print) DOM element to Console") : WI.UIString("Log Node", "Log (print) DOM node to Console");
            contextMenu.appendItem(label, () => {
                WI.RemoteObject.resolveNode(domNode, WI.RuntimeManager.ConsoleObjectGroup).then((remoteObject) => {
                    let text = isElement ? WI.UIString("Selected Element", "Selected DOM element") : WI.UIString("Selected Node", "Selected DOM node");
                    const addSpecialUserLogClass = true;
                    WI.consoleLogViewController.appendImmediateExecutionWithResult(text, remoteObject, addSpecialUserLogClass);
                });
            });
        }

        if (!options.excludeRevealElement && InspectorBackend.hasDomain("DOM") && attached) {
            contextMenu.appendItem(WI.repeatedUIString.revealInDOMTree(), () => {
                WI.domManager.inspectElement(domNode.id, {
                    initiatorHint: WI.TabBrowser.TabNavigationInitiator.ContextMenu,
                });
            });
        }

        if (InspectorBackend.hasDomain("LayerTree") && attached) {
            contextMenu.appendItem(WI.UIString("Reveal in Layers Tab", "Open Layers tab and select the layer corresponding to this node"), () => {
                WI.showLayersTab({
                    nodeToSelect: domNode,
                    initiatorHint: WI.TabBrowser.TabNavigationInitiator.ContextMenu,
                });
            });
        }

        if (InspectorBackend.hasCommand("Page.snapshotNode") && attached) {
            contextMenu.appendItem(WI.UIString("Capture Screenshot", "Capture screenshot of the selected DOM node"), () => {
                let target = WI.assumingMainTarget();
                target.PageAgent.snapshotNode(domNode.id, (error, dataURL) => {
                    if (error) {
                        const target = WI.mainTarget;
                        const source = WI.ConsoleMessage.MessageSource.Other;
                        const level = WI.ConsoleMessage.MessageLevel.Error;
                        let consoleMessage = new WI.ConsoleMessage(target, source, level, error);
                        consoleMessage.shouldRevealConsole = true;

                        WI.consoleLogViewController.appendConsoleMessage(consoleMessage);
                        return;
                    }

                    WI.FileUtilities.save({
                        content: parseDataURL(dataURL).data,
                        base64Encoded: true,
                        suggestedName: WI.FileUtilities.screenshotString() + ".png",
                    });
                });
            });
        }

        if (isElement && attached) {
            contextMenu.appendItem(WI.UIString("Scroll into View", "Scroll selected DOM node into view on the inspected web page"), () => {
                domNode.scrollIntoView();
            });
        }

        contextMenu.appendSeparator();
    }
};

WI.appendContextMenuItemsForDOMNodeBreakpoints = function(contextMenu, domNode, options = {})
{
    if (contextMenu.__domBreakpointItemsAdded)
        return;

    contextMenu.__domBreakpointItemsAdded = true;

    let breakpoints = WI.domDebuggerManager.domBreakpointsForNode(domNode);

    contextMenu.appendSeparator();

    let subMenu = contextMenu.appendSubMenuItem(WI.UIString("Break on"));

    for (let type of Object.values(WI.DOMBreakpoint.Type)) {
        let label = WI.DOMBreakpointTreeElement.displayNameForType(type);
        let breakpoint = breakpoints.find((breakpoint) => breakpoint.type === type);

        subMenu.appendCheckboxItem(label, function() {
            if (breakpoint)
                WI.domDebuggerManager.removeDOMBreakpoint(breakpoint);
            else
                WI.domDebuggerManager.addDOMBreakpoint(new WI.DOMBreakpoint(domNode, type));
        }, !!breakpoint);
    }

    contextMenu.appendSeparator();

    if (breakpoints.length) {
        let shouldEnable = breakpoints.some((breakpoint) => breakpoint.disabled);
        contextMenu.appendItem(shouldEnable ? WI.UIString("Enable Breakpoint") : WI.UIString("Disable Breakpoint"), () => {
            for (let breakpoint of breakpoints)
                breakpoint.disabled = !shouldEnable;
        });

        contextMenu.appendItem(WI.UIString("Delete Breakpoint"), () => {
            for (let breakpoint of breakpoints)
                WI.domDebuggerManager.removeDOMBreakpoint(breakpoint);
        });

        contextMenu.appendSeparator();
    }

    let subtreeBreakpoints = WI.domDebuggerManager.domBreakpointsInSubtree(domNode);
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
