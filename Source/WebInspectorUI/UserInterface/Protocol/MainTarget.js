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
        super("main", "", WI.Target.Type.Main, InspectorBackend.mainConnection);

        let displayName = WI.debuggableType === WI.DebuggableType.Web ? WI.UIString("Main Frame") : this.displayName;
        this._executionContext = new WI.ExecutionContext(this, WI.RuntimeManager.TopLevelContextExecutionIdentifier, displayName, true, null);
    }

    // Protected (Target)

    get displayName()
    {
        switch (WI.debuggableType) {
        case WI.DebuggableType.Web:
            return WI.UIString("Page");
        case WI.DebuggableType.JavaScript:
            return WI.UIString("JavaScript Context");
        default:
            console.error("Unexpected debuggable type: ", WI.debuggableType);
            return WI.UIString("Main");
        }
    }

    get mainResource()
    {
        let mainFrame = WI.frameResourceManager.mainFrame;
        return mainFrame ? mainFrame.mainResource : null;
    }
};
