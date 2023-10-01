/*
 * Copyright (C) 2007 Apple Inc.  All rights reserved.
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
#include "RemoveFormatCommand.h"

#include "ApplyStyleCommand.h"
#include "Element.h"
#include "FrameSelection.h"
#include "HTMLNames.h"
#include "LocalFrame.h"
#include "MutableStyleProperties.h"
#include "NodeName.h"
#include <wtf/NeverDestroyed.h>
#include <wtf/RobinHoodHashSet.h>

namespace WebCore {

using namespace HTMLNames;

RemoveFormatCommand::RemoveFormatCommand(Ref<Document>&& document)
    : CompositeEditCommand(WTFMove(document))
{
}

static bool isElementForRemoveFormatCommand(const Element* element)
{
    using namespace ElementNames;

    switch (element->elementName()) {
    case HTML::acronym:
    case HTML::b:
    case HTML::bdo:
    case HTML::big:
    case HTML::cite:
    case HTML::code:
    case HTML::dfn:
    case HTML::em:
    case HTML::font:
    case HTML::i:
    case HTML::ins:
    case HTML::kbd:
    case HTML::nobr:
    case HTML::q:
    case HTML::s:
    case HTML::samp:
    case HTML::small_:
    case HTML::strike:
    case HTML::strong:
    case HTML::sub:
    case HTML::sup:
    case HTML::tt:
    case HTML::u:
    case HTML::var:
        return true;
    default:
        break;
    }
    return false;
}

void RemoveFormatCommand::doApply()
{
    if (endingSelection().isNoneOrOrphaned())
        return;

    // Get the default style for this editable root, it's the style that we'll give the
    // content that we're operating on.
    auto defaultStyle = EditingStyle::create(endingSelection().rootEditableElement());

    // We want to remove everything but transparent background.
    // FIXME: We shouldn't access style().
    defaultStyle->style()->setProperty(CSSPropertyBackgroundColor, CSSValueTransparent);

    applyCommandToComposite(ApplyStyleCommand::create(protectedDocument(), defaultStyle.ptr(), isElementForRemoveFormatCommand, editingAction()));
}

}
