/*
 * Copyright (C) 2026 Squarespace, Inc. www.squarespace.com
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

#include "config.h"
#include "LazyLoading.h"

#include "DocumentQuirks.h"
#include "ElementInlines.h"
#include "FrameDestructionObserverInlines.h"
#include "HTMLIFrameElement.h"
#include "HTMLImageElement.h"
#include "HTMLNames.h"
#include "LocalFrame.h"
#include "NodeDocument.h"
#include "Quirks.h"
#include "ScriptController.h"
#include "Settings.h"
#include <wtf/text/StringView.h>

#if ENABLE(VIDEO)
#include "HTMLMediaElement.h"
#endif

namespace WebCore {

namespace LazyLoading {

bool hasLazyAttributeValue(StringView attributeValue)
{
    return equalLettersIgnoringASCIICase(attributeValue, "lazy"_s);
}

bool isInLazyState(const Element& element)
{
    return hasLazyAttributeValue(element.attributeWithoutSynchronization(HTMLNames::loadingAttr));
}

bool isLazyLoadingEnabled(const Element& element)
{
    Ref document = element.document();

    if (is<HTMLImageElement>(element))
        return document->settings().lazyImageLoadingEnabled();

    if (is<HTMLIFrameElement>(element))
        return document->settings().lazyIframeLoadingEnabled() && !document->quirks().shouldDisableLazyIframeLoadingQuirk();

#if ENABLE(VIDEO)
    if (is<HTMLMediaElement>(element))
        return document->settings().lazyMediaLoadingEnabled();
#endif

    return false;
}

bool isLazyLoadable(const Element& element)
{
    if (!isLazyLoadingEnabled(element))
        return false;

    // https://html.spec.whatwg.org/multipage/urls-and-fetching.html#will-lazy-load-element-steps
    RefPtr frame = element.document().frame();
    if (!frame || !protect(frame->script())->canExecuteScripts(ReasonForCallingCanExecuteScripts::NotAboutToExecuteScript))
        return false;

    return isInLazyState(element);
}

} // namespace LazyLoading

} // namespace WebCore
