/*
 * Copyright (C) 2016 Devin Rousso <dcrousso+webkit@gmail.com>. All rights reserved.
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

    contextMenu.appendSeparator();

    if (sourceCode.url) {
        contextMenu.appendItem(WI.UIString("Open in New Tab"), () => {
            const frame = null;
            WI.openURL(sourceCode.url, frame, {alwaysOpenExternally: true});
        });

        if (WI.frameResourceManager.resourceForURL(sourceCode.url) && !WI.isShowingResourcesTab()) {
            contextMenu.appendItem(WI.UIString("Reveal in Resources Tab"), () => {
                const options = {ignoreNetworkTab: true};
                if (location)
                    WI.showSourceCodeLocation(location, options);
                else
                    WI.showSourceCode(sourceCode, options);
            });
        }

        contextMenu.appendItem(WI.UIString("Copy Link Address"), () => {
            InspectorFrontendHost.copyText(sourceCode.url);
        });
    }

    if (sourceCode instanceof WI.Resource) {
        if (sourceCode.urlComponents.scheme !== "data") {
            contextMenu.appendItem(WI.UIString("Copy as cURL"), () => {
                sourceCode.generateCURLCommand();
            });
        }
    }

    contextMenu.appendItem(WI.UIString("Save File"), () => {
        sourceCode.requestContent().then(() => {
            const forceSaveAs = true;
            WI.saveDataToFile({
                url: sourceCode.url || "",
                content: sourceCode.content
            }, forceSaveAs);
        });
    });

    contextMenu.appendSeparator();

    if (location && (sourceCode instanceof WI.Script || (sourceCode instanceof WI.Resource && sourceCode.type === WI.Resource.Type.Script))) {
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

        contextMenu.appendSeparator();
    }
};

WI.appendContextMenuItemsForDOMNode = function(contextMenu, domNode, options = {})
{
    console.assert(contextMenu instanceof WI.ContextMenu);
    if (!(contextMenu instanceof WI.ContextMenu))
        return;

    console.assert(domNode instanceof WI.DOMNode);
    if (!(domNode instanceof WI.DOMNode))
        return;

    let isElement = domNode.nodeType() === Node.ELEMENT_NODE;
    if (isElement) {
        contextMenu.appendItem(WI.UIString("Scroll Into View"), () => {
            domNode.scrollIntoView();
        });
    }

    contextMenu.appendSeparator();

    if (domNode.ownerDocument && isElement) {
        contextMenu.appendItem(WI.UIString("Copy Selector Path"), () => {
            let cssPath = WI.cssPath(domNode);
            InspectorFrontendHost.copyText(cssPath);
        });
    }

    if (domNode.ownerDocument && !domNode.isPseudoElement()) {
        contextMenu.appendItem(WI.UIString("Copy XPath"), () => {
            let xpath = WI.xpath(domNode);
            InspectorFrontendHost.copyText(xpath);
        });
    }

    if (domNode.isCustomElement()) {
        contextMenu.appendSeparator();
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

            function didGetProperty(error, result, wasThrown) {
                if (error || result.type !== "function")
                    return;

                DebuggerAgent.getFunctionDetails(result.objectId, didGetFunctionDetails);
                result.release();
            }

            function didResolveNode(remoteObject) {
                if (!remoteObject)
                    return;

                remoteObject.getProperty("constructor", didGetProperty);
                remoteObject.release();
            }

            WI.RemoteObject.resolveNode(domNode, "", didResolveNode);
        });
    }

    if (WI.domDebuggerManager.supported && isElement && !domNode.isPseudoElement() && domNode.ownerDocument) {
        contextMenu.appendSeparator();

        const allowEditing = false;
        WI.DOMBreakpointTreeController.appendBreakpointContextMenuItems(contextMenu, domNode, allowEditing);
    }

    contextMenu.appendSeparator();

    if (!options.excludeRevealElement && domNode.ownerDocument) {
        contextMenu.appendItem(WI.UIString("Reveal in DOM Tree"), () => {
            WI.domTreeManager.inspectElement(domNode.id);
        });
    }

    if (!options.excludeLogElement && !domNode.isInUserAgentShadowTree() && !domNode.isPseudoElement()) {
        let label = isElement ? WI.UIString("Log Element") : WI.UIString("Log Node");
        contextMenu.appendItem(label, () => {
            WI.RemoteObject.resolveNode(domNode, WI.RuntimeManager.ConsoleObjectGroup, (remoteObject) => {
                if (!remoteObject)
                    return;

                let text = isElement ? WI.UIString("Selected Element") : WI.UIString("Selected Node");
                const addSpecialUserLogClass = true;
                WI.consoleLogViewController.appendImmediateExecutionWithResult(text, remoteObject, addSpecialUserLogClass);
            });
        });
    }

    if (window.PageAgent) {
        contextMenu.appendItem(WI.UIString("Capture Element Screenshot"), () => {
            PageAgent.snapshotNode(domNode.id, (error, dataURL) => {
                if (error) {
                    const target = WI.mainTarget;
                    const source = WI.ConsoleMessage.MessageSource.Other;
                    const level = WI.ConsoleMessage.MessageLevel.Error;
                    let consoleMessage = new WI.ConsoleMessage(target, source, level, error);
                    consoleMessage.shouldRevealConsole = true;

                    WI.consoleLogViewController.appendConsoleMessage(consoleMessage);
                    return;
                }

                let date = new Date;
                let values = [
                    date.getFullYear(),
                    Number.zeroPad(date.getMonth() + 1, 2),
                    Number.zeroPad(date.getDate(), 2),
                    Number.zeroPad(date.getHours(), 2),
                    Number.zeroPad(date.getMinutes(), 2),
                    Number.zeroPad(date.getSeconds(), 2),
                ];
                let filename = WI.UIString("Screen Shot %s-%s-%s at %s.%s.%s").format(...values);
                WI.saveDataToFile({
                    url: encodeURI(`web-inspector:///${filename}.png`),
                    content: parseDataURL(dataURL).data,
                    base64Encoded: true,
                });
            });
        });
    }
};
