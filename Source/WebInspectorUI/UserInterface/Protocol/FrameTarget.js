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

        WI.networkManager.addEventListener(WI.NetworkManager.Event.FrameWasAdded, this._handleFrameWasAdded, this);
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

    // Computed at read time: the WI.Frame for the frame id can be replaced when the resource tree
    // is rebuilt, so a stored name goes stale.
    get displayName()
    {
        return this._executionContext?.frame?.url || this.name;
    }

    addExecutionContext(context)
    {
        // On navigation, a new Normal context replaces all prior contexts.
        if (context.type === WI.ExecutionContext.Type.Normal && this._executionContext) {
            this._executionContextList.clear();
            this._executionContext = null;
        }

        if (!this._executionContextList.add(context))
            return;

        if (context.type === WI.ExecutionContext.Type.Normal) {
            this._executionContext = context;
            this._setNameSourceFrame(context.frame);
            this.dispatchEventToListeners(WI.Target.Event.NameChanged);
        }

        this.dispatchEventToListeners(WI.FrameTarget.Event.ExecutionContextAdded, {context});
    }

    ensureTargetExecutionContext()
    {
        this._targetExecutionContextPromise ||= this.executionContext
            ? Promise.resolve()
            : this.awaitEvent(WI.FrameTarget.Event.ExecutionContextAdded);
        return this._targetExecutionContextPromise;
    }

    clearExecutionContexts()
    {
        this._executionContextList.clear();
        this._executionContext = null;
        this._targetExecutionContextPromise = null;
        this._setNameSourceFrame(null);
    }

    destroy()
    {
        WI.networkManager.removeEventListener(WI.NetworkManager.Event.FrameWasAdded, this._handleFrameWasAdded, this);
        this._setNameSourceFrame(null);

        super.destroy();
    }

    // Private

    _handleFrameWasAdded(event)
    {
        let frame = event.data.frame;
        if (!this._executionContext || frame.id !== this._executionContext.frameId)
            return;

        this._setNameSourceFrame(frame);
        this.dispatchEventToListeners(WI.Target.Event.NameChanged);
    }

    _handleNameSourceMainResourceDidChange(event)
    {
        this._setNameSourceResource(this._nameSourceFrame.mainResource);
        this.dispatchEventToListeners(WI.Target.Event.NameChanged);
    }

    _handleNameSourceResourceURLDidChange(event)
    {
        this.dispatchEventToListeners(WI.Target.Event.NameChanged);
    }

    _setNameSourceFrame(frame)
    {
        if (this._nameSourceFrame === frame)
            return;
        this._nameSourceFrame?.removeEventListener(WI.Frame.Event.MainResourceDidChange, this._handleNameSourceMainResourceDidChange, this);
        this._setNameSourceResource(null);
        this._nameSourceFrame = frame;
        if (this._nameSourceFrame) {
            this._nameSourceFrame.addEventListener(WI.Frame.Event.MainResourceDidChange, this._handleNameSourceMainResourceDidChange, this);
            this._setNameSourceResource(this._nameSourceFrame.mainResource);
        }
    }

    // A placeholder main resource's URL is corrected in place, without a MainResourceDidChange.
    _setNameSourceResource(resource)
    {
        if (this._nameSourceResource === resource)
            return;
        this._nameSourceResource?.removeEventListener(WI.Resource.Event.URLDidChange, this._handleNameSourceResourceURLDidChange, this);
        this._nameSourceResource = resource;
        this._nameSourceResource?.addEventListener(WI.Resource.Event.URLDidChange, this._handleNameSourceResourceURLDidChange, this);
    }
};

WI.FrameTarget.Event = {
    ExecutionContextAdded: "frame-target-execution-context-added",
};
