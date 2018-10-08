/*
 * Copyright (C) 2009 Apple Inc. All rights reserved.
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
#include "StyleMedia.h"

#include "Document.h"
#include "Frame.h"
#include "FrameView.h"
#include "MediaList.h"
#include "MediaQueryEvaluator.h"
#include "MediaQueryParser.h"
#include "NodeRenderStyle.h"
#include "RenderElement.h"
#include "StyleResolver.h"
#include "StyleScope.h"

namespace WebCore {

StyleMedia::StyleMedia(DOMWindow& window)
    : DOMWindowProperty(&window)
{
}

String StyleMedia::type() const
{
    auto* frame = this->frame();
    FrameView* view = frame ? frame->view() : nullptr;
    if (view)
        return view->mediaType();

    return String();
}

bool StyleMedia::matchMedium(const String& query) const
{
    auto* frame = this->frame();
    if (!frame)
        return false;

    Document* document = frame->document();
    ASSERT(document);
    Element* documentElement = document->documentElement();
    if (!documentElement)
        return false;

    auto rootStyle = document->styleScope().resolver().styleForElement(*documentElement, document->renderStyle(), nullptr, RuleMatchingBehavior::MatchOnlyUserAgentRules).renderStyle;

    auto media = MediaQuerySet::create(query, MediaQueryParserContext(*document));

    return MediaQueryEvaluator { type(), *document, rootStyle.get() }.evaluate(media.get());
}

} // namespace WebCore
