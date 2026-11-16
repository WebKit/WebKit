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
        // Per-edit LIFOs of targets. Each successful didEdit pushes one entry; undo/redo
        // move entries between the two stacks. No deduplication: the stacks mirror each
        // backend agent's per-process InspectorHistory so that Cmd+Z reverses edits in
        // the order they were made across frames.
        this._undoTargetStack = [];
        this._redoTargetStack = [];

        // Chained promise that serializes undo/redo dispatch. Without this, back-to-back
        // Cmd+Z presses could send overlapping DOM.undo/DOM.redo commands whose responses
        // resolve out of order (especially across frame targets in different processes),
        // corrupting the order entries land back on the opposite stack.
        this._chainedOperationsPromise = Promise.resolve();

        WI.targetManager.addEventListener(WI.TargetManager.Event.TargetRemoved, this._handleTargetRemoved, this);
    }

    // Public

    // didEdit()/markUndoableState() only push a target and fire a command; they don't need to
    // wait on anything, so they stay synchronous. undo()/redo() are async because they must wait
    // for their turn in _chainedOperationsPromise and then for the backend response before it's
    // safe to move the target to the opposite stack — see _performOperationSoon() below.
    didEdit(target)
    {
        // A null target means "the edit's target is unknown," not "the edit belongs to main." Defaulting to
        // main here would clobber a previously-recorded frame target and misroute the next Cmd+Z, so no-op
        // and keep the existing routing. markUndoableState() is what supplies the concrete target.
        if (!target)
            return;
        this._undoTargetStack.push(target);
        this._redoTargetStack = [];
    }

    markUndoableState(target)
    {
        // Unlike didEdit(), a missing target here is a real page-level edit (e.g. a DOMNode with no owning
        // frame target, or a CSS edit while FrameCSSAgent does not yet exist), which genuinely belongs to
        // main. Resolve it so the edit is both recorded and dispatched against a concrete target.
        //
        // Assumes each call here corresponds to a nonempty backend undo group: InspectorHistory::undo()/redo()
        // collapse over consecutive UndoableStateMark entries, so calling this without a preceding real edit
        // action produces an empty backend group while still pushing a stack entry here, breaking the 1:1
        // correspondence the multi-target undo/redo stacks rely on.
        target ||= WI.assumingMainTarget();
        this.didEdit(target);
        if (target.hasCommand("DOM.markUndoableState"))
            target.DOMAgent.markUndoableState();
    }

    undo()
    {
        return this._performOperationSoon(() => this._undo());
    }

    redo()
    {
        return this._performOperationSoon(() => this._redo());
    }

    // Private

    // undo()/redo() are invoked as fire-and-forget from keyboard shortcut handlers, so
    // no caller is positioned to catch a rejection. Log and swallow failures here instead
    // of leaving an unhandled rejection for something downstream to trip over; the chain
    // itself is kept alive by catching separately so one failed operation can't wedge
    // every operation queued after it.
    _performOperationSoon(operation)
    {
        let result = this._chainedOperationsPromise.then(operation);
        this._chainedOperationsPromise = result.catch(() => {});
        return result.catch((error) => { console.error(error); });
    }

    async _undo()
    {
        let target = this._undoTargetStack.lastValue;
        if (!target) {
            // Older Page-only backend without the per-frame DOM.undo introduced for Site Isolation:
            // nothing was ever pushed, so dispatch against main. The main target definitely supports
            // DOM.undo (assuming ITML has been deprecated), so no hasCommand() guard is needed.
            await WI.assumingMainTarget().DOMAgent.undo();
            return;
        }

        if (!target.hasCommand("DOM.undo")) {
            console.assert(false, "Edited target should support DOM.undo", target);
            return;
        }
        this._undoTargetStack.pop();
        await target.DOMAgent.undo();
        this._redoTargetStack.push(target);
    }

    async _redo()
    {
        let target = this._redoTargetStack.lastValue;
        if (!target) {
            // Same older-backend fallback as _undo() above.
            await WI.assumingMainTarget().DOMAgent.redo();
            return;
        }

        if (!target.hasCommand("DOM.redo")) {
            console.assert(false, "Edited target should support DOM.redo", target);
            return;
        }
        this._redoTargetStack.pop();
        await target.DOMAgent.redo();
        this._undoTargetStack.push(target);
    }

    _handleTargetRemoved(event)
    {
        let removed = event.data.target;
        this._undoTargetStack = this._undoTargetStack.filter((t) => t !== removed);
        this._redoTargetStack = this._redoTargetStack.filter((t) => t !== removed);
    }
};
