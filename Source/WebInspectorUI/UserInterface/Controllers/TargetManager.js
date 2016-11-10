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

WebInspector.TargetManager = class TargetManager extends WebInspector.Object
{
    constructor()
    {
        super();

        console.assert(WebInspector.mainTarget);

        this._targets = new Set([WebInspector.mainTarget]);
    }

    // Public

    get targets()
    {
        return this._targets;
    }

    targetForIdentifier(targetId)
    {
        if (!targetId)
            return null;

        for (let target of this._targets) {
            if (target.identifier === targetId)
                return target;
        }

        return null;
    }

    addTarget(target)
    {
        this._targets.add(target);

        this.dispatchEventToListeners(WebInspector.TargetManager.Event.TargetAdded, {target});
    }

    removeTarget(target)
    {
        this._targets.delete(target);

        this.dispatchEventToListeners(WebInspector.TargetManager.Event.TargetRemoved, {target});
    }
};

WebInspector.TargetManager.Event = {
    TargetAdded: Symbol("target-manager-target-added"),
    TargetRemoved: Symbol("target-manager-target-removed"),
};
