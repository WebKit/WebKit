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

WebInspector.appendContextMenuItemsForResource = function(contextMenu, resource)
{

    console.assert(contextMenu instanceof WebInspector.ContextMenu);
    if (!(contextMenu instanceof WebInspector.ContextMenu))
        return;

    console.assert(resource instanceof WebInspector.Resource);
    if (!(resource instanceof WebInspector.Resource))
        return;

    contextMenu.appendItem(WebInspector.UIString("Open in New Tab"), () => {
        const frame = null;
        const alwaysOpenExternally = true;
        WebInspector.openURL(resource.url, frame, alwaysOpenExternally);
    });

    contextMenu.appendItem(WebInspector.UIString("Copy Link Address"), () => {
        InspectorFrontendHost.copyText(resource.url);
    });

    if (resource.urlComponents.scheme !== "data") {
        contextMenu.appendItem(WebInspector.UIString("Copy as cURL"), () => {
            resource.generateCURLCommand();
        });
    }

    contextMenu.appendItem(WebInspector.UIString("Save File"), () => {
        resource.requestContent().then(() => {
            WebInspector.saveDataToFile({
                url: resource.url,
                content: resource.content
            });
        });
    });
};
