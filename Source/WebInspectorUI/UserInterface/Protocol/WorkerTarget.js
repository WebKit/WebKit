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

WebInspector.WorkerTarget = class WorkerTarget extends WebInspector.Target
{
    constructor(workerId, name, connection)
    {
        super(workerId, name, WebInspector.Target.Type.Worker, connection);

        WebInspector.frameResourceManager.adoptOrphanedResourcesForTarget(this);

        if (this.RuntimeAgent) {
            this._executionContext = new WebInspector.ExecutionContext(this, WebInspector.RuntimeManager.TopLevelContextExecutionIdentifier, this.displayName, false, null);
            this.RuntimeAgent.enable();
            if (WebInspector.showJavaScriptTypeInformationSetting && WebInspector.showJavaScriptTypeInformationSetting.value)
                this.RuntimeAgent.enableTypeProfiler();
            if (WebInspector.enableControlFlowProfilerSetting && WebInspector.enableControlFlowProfilerSetting.value)
                this.RuntimeAgent.enableControlFlowProfiler();
        }

        if (this.DebuggerAgent)
            WebInspector.debuggerManager.initializeTarget(this);

        if (this.ConsoleAgent)
            this.ConsoleAgent.enable();

        if (this.HeapAgent)
            this.HeapAgent.enable();
    }

    // Protected (Target)

    get displayName()
    {
        return WebInspector.displayNameForURL(this._name);
    }
};
