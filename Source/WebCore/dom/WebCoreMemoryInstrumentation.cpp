/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "WebCoreMemoryInstrumentation.h"

#include "RenderBlock.h"
#include "RenderBox.h"

namespace WebCore {

MemoryObjectType WebCoreMemoryTypes::Page = "DOM";
MemoryObjectType WebCoreMemoryTypes::DOM = "DOM";
MemoryObjectType WebCoreMemoryTypes::CSS = "CSS";
MemoryObjectType WebCoreMemoryTypes::Binding = "DOM";
MemoryObjectType WebCoreMemoryTypes::RenderingStructures = "Rendering";

MemoryObjectType WebCoreMemoryTypes::MemoryCacheStructures = "Resources";
MemoryObjectType WebCoreMemoryTypes::CachedResource = "Resources";
MemoryObjectType WebCoreMemoryTypes::CachedResourceRaw = "Resources";
MemoryObjectType WebCoreMemoryTypes::CachedResourceCSS = "Resources";
MemoryObjectType WebCoreMemoryTypes::CachedResourceFont = "Resources";
MemoryObjectType WebCoreMemoryTypes::CachedResourceImage = "Resources";
MemoryObjectType WebCoreMemoryTypes::CachedResourceScript = "Resources";
MemoryObjectType WebCoreMemoryTypes::CachedResourceSVG = "Resources";
MemoryObjectType WebCoreMemoryTypes::CachedResourceShader = "Resources";
MemoryObjectType WebCoreMemoryTypes::CachedResourceXSLT = "Resources";

MemoryObjectType WebCoreMemoryTypes::ExternalStrings = "JSExternalResources";
MemoryObjectType WebCoreMemoryTypes::ExternalArrays = "JSExternalResources";

MemoryObjectType WebCoreMemoryTypes::Inspector = "WebInspector";
MemoryObjectType WebCoreMemoryTypes::InspectorController = "WebInspector";
MemoryObjectType WebCoreMemoryTypes::InspectorMemoryAgent = "WebInspector";
MemoryObjectType WebCoreMemoryTypes::InspectorDOMStorageAgent = "WebInspector";
MemoryObjectType WebCoreMemoryTypes::InspectorDOMStorageResources = "WebInspector";
MemoryObjectType WebCoreMemoryTypes::InspectorOverlay = "WebInspector";
MemoryObjectType WebCoreMemoryTypes::InspectorProfilerAgent = "WebInspector";
MemoryObjectType WebCoreMemoryTypes::InspectorDebuggerAgent = "WebInspector";
MemoryObjectType WebCoreMemoryTypes::InspectorResourceAgent = "WebInspector";

MemoryObjectType WebCoreMemoryTypes::JSHeapUsed = "JSHeap.Used";
MemoryObjectType WebCoreMemoryTypes::JSHeapUnused = "JSHeap.Unused";

MemoryObjectType WebCoreMemoryTypes::DOMStorageCache = "DOMStorageCache";

MemoryObjectType WebCoreMemoryTypes::RenderTreeUsed = "RenderTree";
MemoryObjectType WebCoreMemoryTypes::RenderTreeUnused = "RenderTree";

MemoryObjectType WebCoreMemoryTypes::ProcessPrivateMemory = "ProcessPrivateMemory";


void WebCoreMemoryInstrumentation::reportStaticMembersMemoryUsage(WTF::MemoryInstrumentation* memoryInstrumentation)
{
    RenderBlock::reportStaticMembersMemoryUsage(memoryInstrumentation);
    RenderBox::reportStaticMembersMemoryUsage(memoryInstrumentation);
}

} // namespace WebCore
