/*
 * Copyright (C) 2017 Apple Inc. All rights reserved.
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

WI.JSONResourceContentView = class JSONResourceContentView extends WI.ResourceContentView
{
    constructor(resource)
    {
        super(resource, "json");

        this._remoteObject = null;
    }

    // Static

    static customContentViewDisplayName()
    {
        return WI.UIString("JSON");
    }

    // Protected

    contentAvailable(content, base64Encoded)
    {
        try {
            JSON.parse(content);
        } catch (e) {
            this.showMessage(WI.UIString("Unable to parse as JSON: %s").format(e.message));
            return;
        }

        const options = {
            expression: "(" + content + ")",
            includeCommandLineAPI: false,
            doNotPauseOnExceptionsAndMuteConsole: true,
            contextId: undefined,
            returnByValue: false,
            generatePreview: true,
        };
        this.resource.target.RuntimeAgent.evaluate.invoke(options, (error, result, wasThrown) => {
            if (error || wasThrown) {
                this.showMessage(WI.UIString("Unable to parse as JSON: %s").format(result.description));
                return;
            }

            this.removeLoadingIndicator();

            this._remoteObject = WI.RemoteObject.fromPayload(result, this.resource.target);

            let objectTree = new WI.ObjectTreeView(this._remoteObject);
            objectTree.showOnlyJSON();
            objectTree.expand();

            this.element.appendChild(objectTree.element);
        }, this.resource.target);
    }

    closed()
    {
        if (this._remoteObject) {
            this._remoteObject.release();
            this._remoteObject = null;
        }
    }
};
