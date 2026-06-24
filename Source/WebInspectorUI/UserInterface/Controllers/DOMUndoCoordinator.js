/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

WI.DOMUndoCoordinator = class DOMUndoCoordinator
{
    constructor()
    {
        // Most recently edited target. Cmd+Z dispatches here. Stays valid until that target is removed.
        this._lastEditTarget = null;

        WI.targetManager.addEventListener(WI.TargetManager.Event.TargetRemoved, this._handleTargetRemoved, this);
    }

    // Public

    didEdit(target)
    {
        // A null target means "the edit's target is unknown," not "the edit belongs to main." Defaulting to
        // main here would clobber a previously-recorded frame target and misroute the next Cmd+Z, so no-op
        // and keep the existing routing. markUndoableState() is what supplies the concrete target.
        if (!target)
            return;
        this._lastEditTarget = target;
    }

    markUndoableState(target)
    {
        // Unlike didEdit(), a missing target here is a real page-level edit (e.g. a DOMNode with no owning
        // frame target, or a CSS edit while FrameCSSAgent does not yet exist), which genuinely belongs to
        // main. Resolve it so the edit is both recorded and dispatched against a concrete target.
        target ||= WI.assumingMainTarget();
        this.didEdit(target);
        if (target.hasCommand("DOM.markUndoableState"))
            target.DOMAgent.markUndoableState();
    }

    undo()
    {
        let target = this._lastEditTarget || WI.assumingMainTarget();
        if (target.hasCommand("DOM.undo"))
            target.DOMAgent.undo();
    }

    redo()
    {
        let target = this._lastEditTarget || WI.assumingMainTarget();
        if (target.hasCommand("DOM.redo"))
            target.DOMAgent.redo();
    }

    // Private

    _handleTargetRemoved(event)
    {
        if (this._lastEditTarget === event.data.target)
            this._lastEditTarget = null;
    }
};
