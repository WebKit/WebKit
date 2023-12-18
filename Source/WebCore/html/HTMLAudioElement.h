/*
 * Copyright (C) 2007, 2010 Apple Inc. All rights reserved.
 * Copyright (C) 2010 Google Inc. All rights reserved.
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

#if ENABLE(VIDEO)

#include "HTMLMediaElement.h"

namespace WebCore {

class Document;

class HTMLAudioElement final : public HTMLMediaElement {
    WTF_MAKE_ISO_ALLOCATED(HTMLAudioElement);
public:
    static Ref<HTMLAudioElement> create(const QualifiedName&, Document&, bool);
    static Ref<HTMLAudioElement> createForLegacyFactoryFunction(Document&, const AtomString& src);

private:
    HTMLAudioElement(const QualifiedName&, Document&, bool);

    PlatformMediaSession::MediaType presentationType() const final { return PlatformMediaSession::MediaType::Audio; }
};

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_BEGIN(WebCore::HTMLAudioElement)
    static bool isType(const WebCore::HTMLMediaElement& element) { return element.hasTagName(WebCore::HTMLNames::audioTag); }
    static bool isType(const WebCore::Element& element)
    {
        auto* mediaElement = dynamicDowncast<WebCore::HTMLMediaElement>(element);
        return mediaElement && isType(*mediaElement);
    }
    static bool isType(const WebCore::Node& node)
    {
        auto* mediaElement = dynamicDowncast<WebCore::HTMLMediaElement>(node);
        return mediaElement && isType(*mediaElement);
    }
SPECIALIZE_TYPE_TRAITS_END()

#endif // ENABLE(VIDEO)
