/*
 * Copyright (C) 2016 Apple Inc. All rights reserved.
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

WI.MainTarget = class MainTarget extends WI.Target
{
    constructor(connection)
    {
        let {type, displayName} = MainTarget.mainConnectionInfo();

        super("main", displayName, type, InspectorBackend.mainConnection);

        this._executionContext = new WI.ExecutionContext(this, WI.RuntimeManager.TopLevelExecutionContextIdentifier, displayName, true, null);
        this._mainResource = null;
    }

    // Static

    static mainConnectionInfo()
    {
        switch (WI.sharedApp.debuggableType) {
        case WI.DebuggableType.JavaScript:
            return {
                type: WI.Target.Type.JSContext,
                displayName: WI.UIString("JavaScript Context"),
            };
        case WI.DebuggableType.ServiceWorker:
            return {
                type: WI.Target.Type.ServiceWorker,
                displayName: WI.UIString("ServiceWorker"),
            };
        case WI.DebuggableType.Web:
            return {
                type: WI.Target.Type.Page,
                displayName: WI.UIString("Page"),
            };
        default:
            console.error("Unexpected debuggable type: ", WI.sharedApp.debuggableType);
            return {
                type: WI.Target.Type.Page,
                displayName: WI.UIString("Main"),
            };
        }
    }

    // Protected (Target)

    get mainResource()
    {
        if (this._mainResource)
            return this._mainResource;

        let mainFrame = WI.frameResourceManager.mainFrame;
        return mainFrame ? mainFrame.mainResource : null;
    }

    set mainResource(resource)
    {
        this._mainResource = resource;
    }
};
