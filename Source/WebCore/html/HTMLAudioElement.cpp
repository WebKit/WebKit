/*
 * Copyright (C) 2007-2017 Apple Inc. All rights reserved.
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

#include "config.h"

#if ENABLE(VIDEO)

#include "HTMLAudioElement.h"

#include "HTMLNames.h"
#include <wtf/IsoMallocInlines.h>

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(HTMLAudioElement);

using namespace HTMLNames;

inline HTMLAudioElement::HTMLAudioElement(const QualifiedName& tagName, Document& document, bool createdByParser)
    : HTMLMediaElement(tagName, document, createdByParser)
{
    ASSERT(hasTagName(audioTag));
}

Ref<HTMLAudioElement> HTMLAudioElement::create(const QualifiedName& tagName, Document& document, bool createdByParser)
{
    auto element = adoptRef(*new HTMLAudioElement(tagName, document, createdByParser));
    element->suspendIfNeeded();
    return element;
}

Ref<HTMLAudioElement> HTMLAudioElement::createForLegacyFactoryFunction(Document& document, const AtomString& src)
{
    auto element = create(audioTag, document, false);
    element->setAttributeWithoutSynchronization(preloadAttr, "auto");
    element->setAttributeWithoutSynchronization(srcAttr, src);
    return element;
}

}

#endif
