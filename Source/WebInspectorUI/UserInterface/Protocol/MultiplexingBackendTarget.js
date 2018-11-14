/*
 * Copyright (C) 2018 Apple Inc. All rights reserved.
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

// This class is used when connecting to a target which multiplexes to other targets.
// The main connection is to a target that, currently, has only a Target agent.
// All other Targets will have their messages be multiplexed through this
// main connection's Target agent.

WI.MultiplexingBackendTarget = class MultiplexingBackendTarget extends WI.Target
{
    constructor()
    {
        const type = WI.Target.Type.Page;
        const displayName = WI.UIString("Page");

        super("multi", displayName, type, InspectorBackend.backendConnection);

        let agents = this._agents;
        this._agents = {
            Target: agents.Target,
        };
    }

    // Target

    initialize()
    {
        // Intentionally do nothing, including not calling super.
        // No agents other than the TargetAgent, nothing to initialize.
    }

    // Protected (Target)

    get name()
    {
        console.error("Called name on a MultiplexingBackendTarget");
        return WI.UIString("Page");
    }

    get executionContext()
    {
        console.error("Called executionContext on a MultiplexingBackendTarget");
        return null;
    }

    get mainResource()
    {
        console.error("Called mainResource on a MultiplexingBackendTarget");
        return null;
    }
};
