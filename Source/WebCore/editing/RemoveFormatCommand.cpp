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
#include "Frame.h"
#include "FrameSelection.h"
#include "HTMLNames.h"
#include "StyleProperties.h"
#include <wtf/NeverDestroyed.h>

namespace WebCore {

using namespace HTMLNames;

RemoveFormatCommand::RemoveFormatCommand(Document& document)
    : CompositeEditCommand(document)
{
}

static bool isElementForRemoveFormatCommand(const Element* element)
{
    static NeverDestroyed<HashSet<QualifiedName>> elements;
    if (elements.get().isEmpty()) {
        elements.get().add(acronymTag);
        elements.get().add(bTag);
        elements.get().add(bdoTag);
        elements.get().add(bigTag);
        elements.get().add(citeTag);
        elements.get().add(codeTag);
        elements.get().add(dfnTag);
        elements.get().add(emTag);
        elements.get().add(fontTag);
        elements.get().add(iTag);
        elements.get().add(insTag);
        elements.get().add(kbdTag);
        elements.get().add(nobrTag);
        elements.get().add(qTag);
        elements.get().add(sTag);
        elements.get().add(sampTag);
        elements.get().add(smallTag);
        elements.get().add(strikeTag);
        elements.get().add(strongTag);
        elements.get().add(subTag);
        elements.get().add(supTag);
        elements.get().add(ttTag);
        elements.get().add(uTag);
        elements.get().add(varTag);
    }
    return elements.get().contains(element->tagQName());
}

void RemoveFormatCommand::doApply()
{
    if (!endingSelection().isNonOrphanedCaretOrRange())
        return;

    // Get the default style for this editable root, it's the style that we'll give the
    // content that we're operating on.
    Node* root = endingSelection().rootEditableElement();
    RefPtr<EditingStyle> defaultStyle = EditingStyle::create(root);

    // We want to remove everything but transparent background.
    // FIXME: We shouldn't access style().
    defaultStyle->style()->setProperty(CSSPropertyBackgroundColor, CSSValueTransparent);

    applyCommandToComposite(ApplyStyleCommand::create(document(), defaultStyle.get(), isElementForRemoveFormatCommand, editingAction()));
}

}
