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

MemoryObjectType WebCoreMemoryTypes::Page = "Page";
MemoryObjectType WebCoreMemoryTypes::DOM = "Page.DOM";
MemoryObjectType WebCoreMemoryTypes::CSS = "Page.CSS";
MemoryObjectType WebCoreMemoryTypes::Binding = "Page.Binding";
MemoryObjectType WebCoreMemoryTypes::RenderingStructures = "Page.Rendering.Structures";

MemoryObjectType WebCoreMemoryTypes::MemoryCacheStructures = "MemoryCache.InternalStructures";
MemoryObjectType WebCoreMemoryTypes::CachedResource = "MemoryCache.Resource";
MemoryObjectType WebCoreMemoryTypes::CachedResourceRaw = "MemoryCache.RawResource";
MemoryObjectType WebCoreMemoryTypes::CachedResourceCSS = "MemoryCache.CSS";
MemoryObjectType WebCoreMemoryTypes::CachedResourceFont = "MemoryCache.Font";
MemoryObjectType WebCoreMemoryTypes::CachedResourceImage = "MemoryCache.Image";
MemoryObjectType WebCoreMemoryTypes::CachedResourceScript = "MemoryCache.Script";
MemoryObjectType WebCoreMemoryTypes::CachedResourceSVG = "MemoryCache.SVG";
MemoryObjectType WebCoreMemoryTypes::CachedResourceShader = "MemoryCache.Shader";
MemoryObjectType WebCoreMemoryTypes::CachedResourceXSLT = "MemoryCache.XSLT";

MemoryObjectType WebCoreMemoryTypes::ExternalStrings = "JSExternalResources.Strings";
MemoryObjectType WebCoreMemoryTypes::ExternalArrays = "JSExternalResources.Arrays";

MemoryObjectType WebCoreMemoryTypes::Inspector = "WebInspector";
MemoryObjectType WebCoreMemoryTypes::InspectorController = "WebInspector.Controller";
MemoryObjectType WebCoreMemoryTypes::InspectorMemoryAgent = "WebInspector.MemoryAgent";
MemoryObjectType WebCoreMemoryTypes::InspectorDOMStorageAgent = "WebInspector.DOMStorageAgent";
MemoryObjectType WebCoreMemoryTypes::InspectorDOMStorageResources = "WebInspector.DOMStorageAgent.Resources";
MemoryObjectType WebCoreMemoryTypes::InspectorOverlay = "WebInspector.Overlay";
MemoryObjectType WebCoreMemoryTypes::InspectorProfilerAgent = "WebInspector.ProfilerAgent";
MemoryObjectType WebCoreMemoryTypes::InspectorDebuggerAgent = "WebInspector.DebuggerAgent";
MemoryObjectType WebCoreMemoryTypes::InspectorResourceAgent = "WebInspector.ResourceAgent";

MemoryObjectType WebCoreMemoryTypes::JSHeapUsed = "JSHeap.Used";
MemoryObjectType WebCoreMemoryTypes::JSHeapUnused = "JSHeap.Unused";

MemoryObjectType WebCoreMemoryTypes::DOMStorageCache = "DOMStorageCache";

MemoryObjectType WebCoreMemoryTypes::RenderTreeUsed = "RenderTree.Used";
MemoryObjectType WebCoreMemoryTypes::RenderTreeUnused = "RenderTree.Unused";

MemoryObjectType WebCoreMemoryTypes::ProcessPrivateMemory = "ProcessPrivateMemory";


void WebCoreMemoryInstrumentation::reportStaticMembersMemoryUsage(WTF::MemoryInstrumentation* memoryInstrumentation)
{
    RenderBlock::reportStaticMembersMemoryUsage(memoryInstrumentation);
    RenderBox::reportStaticMembersMemoryUsage(memoryInstrumentation);
}

} // namespace WebCore
