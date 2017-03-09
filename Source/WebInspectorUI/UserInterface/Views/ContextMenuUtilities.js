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

WebInspector.appendContextMenuItemsForSourceCode = function(contextMenu, sourceCodeOrLocation)
{
    console.assert(contextMenu instanceof WebInspector.ContextMenu);
    if (!(contextMenu instanceof WebInspector.ContextMenu))
        return;

    let sourceCode = sourceCodeOrLocation;
    let location = null;
    if (sourceCodeOrLocation instanceof WebInspector.SourceCodeLocation) {
        sourceCode = sourceCodeOrLocation.sourceCode;
        location = sourceCodeOrLocation;
    }

    console.assert(sourceCode instanceof WebInspector.SourceCode);
    if (!(sourceCode instanceof WebInspector.SourceCode))
        return;

    contextMenu.appendSeparator();

    if (sourceCode.url) {
        contextMenu.appendItem(WebInspector.UIString("Open in New Tab"), () => {
            const frame = null;
            WebInspector.openURL(sourceCode.url, frame, {alwaysOpenExternally: true});
        });

        if (WebInspector.frameResourceManager.resourceForURL(sourceCode.url) && !WebInspector.isShowingResourcesTab()) {
            contextMenu.appendItem(WebInspector.UIString("Reveal in Resources Tab"), () => {
                const options = {ignoreNetworkTab: true};
                if (location)
                    WebInspector.showSourceCodeLocation(location, options);
                else
                    WebInspector.showSourceCode(sourceCode, options);
            });
        }

        contextMenu.appendItem(WebInspector.UIString("Copy Link Address"), () => {
            InspectorFrontendHost.copyText(sourceCode.url);
        });
    }

    if (sourceCode instanceof WebInspector.Resource) {
        if (sourceCode.urlComponents.scheme !== "data") {
            contextMenu.appendItem(WebInspector.UIString("Copy as cURL"), () => {
                sourceCode.generateCURLCommand();
            });
        }
    }

    contextMenu.appendItem(WebInspector.UIString("Save File"), () => {
        sourceCode.requestContent().then(() => {
            WebInspector.saveDataToFile({
                url: sourceCode.url || "",
                content: sourceCode.content
            });
        });
    });

    contextMenu.appendSeparator();

    if (location && (sourceCode instanceof WebInspector.Script || (sourceCode instanceof WebInspector.Resource && sourceCode.type === WebInspector.Resource.Type.Script))) {
        let existingBreakpoint = WebInspector.debuggerManager.breakpointForSourceCodeLocation(location);
        if (existingBreakpoint) {
            contextMenu.appendItem(WebInspector.UIString("Delete Breakpoint"), () => {
                WebInspector.debuggerManager.removeBreakpoint(existingBreakpoint);
            });
        } else {
            contextMenu.appendItem(WebInspector.UIString("Add Breakpoint"), () => {
                WebInspector.debuggerManager.addBreakpoint(new WebInspector.Breakpoint(location));
            });
        }

        contextMenu.appendSeparator();
    }
};
