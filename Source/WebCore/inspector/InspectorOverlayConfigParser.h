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

#pragma once

#include "InspectorOverlay.h"
#include <JavaScriptCore/InspectorBackendDispatcher.h>
#include <wtf/Forward.h>

namespace WebCore {

// Shared by InspectorDOMAgent and FrameDOMAgent: converting the protocol's highlight configuration
// objects into the overlay's config structs. Depends on InspectorOverlay.h, so WebCore-internal.

std::optional<Color> parseInspectorOverlayConfigColor(RefPtr<JSON::Object>&&);
bool parseInspectorQuad(Ref<JSON::Array>&&, FloatQuad*);

Inspector::CommandResult<std::unique_ptr<InspectorOverlay::Highlight::Config>> highlightConfigFromInspectorObject(RefPtr<JSON::Object>&&);

// A null configuration object is not an error: the protocol parameter is optional, so it yields an
// engaged result holding std::nullopt. Only a malformed object produces an error.
Inspector::CommandResult<std::optional<InspectorOverlay::Grid::Config>> gridOverlayConfigFromInspectorObject(RefPtr<JSON::Object>&&);
Inspector::CommandResult<std::optional<InspectorOverlay::Flex::Config>> flexOverlayConfigFromInspectorObject(RefPtr<JSON::Object>&&);

} // namespace WebCore
