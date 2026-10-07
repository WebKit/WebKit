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

#pragma once

#include <WebCore/FrameIdentifier.h>
#include <WebCore/TextManipulationItemIdentifier.h>
#include <wtf/Markable.h>
#include <wtf/ObjectIdentifier.h>
#include <wtf/Vector.h>
#include <wtf/WeakPtr.h>

namespace WebCore {

class Element;
class WeakPtrImplWithEventTargetData;

enum class TextManipulationGroupIdentifierType { };
using TextManipulationGroupIdentifier = ObjectIdentifier<TextManipulationGroupIdentifierType>;

enum class TextManipulationGroupKind : uint8_t {
    PageHeading,
    Prose,
    Navigation,
    Banner,
    Footer,
    Sidebar,
    ViewportConstrained,
    Dialog,
    Other,
};

struct TextManipulationGroup {
    Markable<TextManipulationGroupIdentifier> identifier;
    TextManipulationGroupKind kind { TextManipulationGroupKind::Other };
    Vector<WeakPtr<Element, WeakPtrImplWithEventTargetData>> rootElements;
    Vector<TextManipulationItemIdentifier> itemIdentifiers;
};

struct TextManipulationGroupUpdate {
    Markable<FrameIdentifier> frameID;
    TextManipulationGroupIdentifier identifier;
    TextManipulationGroupKind kind { TextManipulationGroupKind::Other };
    Vector<TextManipulationItemIdentifier> itemIdentifiers;
};

} // namespace WebCore
