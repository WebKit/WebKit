/*
 * Copyright (C) 2025 Apple Inc. All rights reserved.
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

WI.FrameTarget = class FrameTarget extends WI.Target
{
    constructor(parentTarget, targetId, name, connection, options = {})
    {
        super(parentTarget, targetId, name, WI.TargetType.Frame, connection, options);

        this._executionContextList = new WI.ExecutionContextList;
    }

    // Public

    get executionContextList()
    {
        return this._executionContextList;
    }

    get executionContext()
    {
        return this._executionContext;
    }

    addExecutionContext(context)
    {
        // On navigation, a new Normal context replaces all prior contexts.
        if (context.type === WI.ExecutionContext.Type.Normal && this._executionContext) {
            this._executionContextList.clear();
            this._executionContext = null;
        }

        this._executionContextList.add(context);

        if (context.type === WI.ExecutionContext.Type.Normal)
            this._executionContext = context;

        this.dispatchEventToListeners(WI.FrameTarget.Event.ExecutionContextAdded, {context});
    }

    clearExecutionContexts()
    {
        this._executionContextList.clear();
        this._executionContext = null;
    }
};

WI.FrameTarget.Event = {
    ExecutionContextAdded: "frame-target-execution-context-added",
};
