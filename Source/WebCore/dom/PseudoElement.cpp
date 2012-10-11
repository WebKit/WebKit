/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
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

#include "PseudoElement.h"

#include "ContentData.h"
#include "NodeRenderStyle.h"
#include "NodeRenderingContext.h"
#include "RenderCounter.h"
#include "RenderImage.h"
#include "RenderImageResourceStyleImage.h"
#include "RenderQuote.h"
#include "RenderTextFragment.h"
#include "StyleInheritedData.h"
#include "Text.h"

namespace WebCore {

const QualifiedName& pseudoElementName()
{
    DEFINE_STATIC_LOCAL(QualifiedName, name, (nullAtom, "<generated>", nullAtom));
    return name;
}

PseudoElement::PseudoElement(Element* parent, PseudoId pseudoId)
    : Element(pseudoElementName(), parent->document(), CreateElement)
    , m_pseudoId(pseudoId)
{
    setParentOrHostNode(parent);
    setHasCustomCallbacks();
}

bool PseudoElement::pseudoRendererIsNeeded(RenderStyle* style)
{
    // Renderers are only needed if there is non empty (or none) content, or if this
    // is a region thread it could also have content flowed into it.
    bool hasContent = style->contentData() && style->contentData()->type() != CONTENT_NONE;
    bool hasRegionThread = !style->regionThread().isEmpty();
    return hasContent || hasRegionThread;
}

PassRefPtr<RenderStyle> PseudoElement::customStyleForRenderer()
{
    return parentOrHostElement()->renderer()->getCachedPseudoStyle(pseudoId());
}

void PseudoElement::attach()
{
    ASSERT(!renderer());

    Element::attach();

    RenderObject* renderer = this->renderer();
    if (!renderer || !renderer->style()->regionThread().isEmpty())
        return;

    ASSERT(renderStyle()->contentData());

    for (const ContentData* content = renderStyle()->contentData(); content; content = content->next()) {
        RenderObject* child = createRendererForContent(content);
        if (!child)
            continue;
        if (renderer->isChildAllowed(child, renderStyle()))
            renderer->addChild(child);
        else
            child->destroy();
    }
}

bool PseudoElement::rendererIsNeeded(const NodeRenderingContext& context)
{
    return Element::rendererIsNeeded(context) && pseudoRendererIsNeeded(context.style());
}

void PseudoElement::updateChildStyle(RenderObject* child) const
{
    // We only manage the style for the generated content which must be text or images.
    if (!child->isText() && !child->isImage())
        return;

    // The style for the RenderTextFragment for first letter is managed by an enclosing block, not by us.
    if (child->style() && child->style()->styleType() == FIRST_LETTER)
        return;

    // Fast path for text where we can share the style. For images we must take the slower
    // inherit path so the width/height of the pseudo element doesn't change the size of the
    // image.
    if (child->isText()) {
        child->setStyle(renderStyle());
        return;
    }

    RefPtr<RenderStyle> style = RenderStyle::create();
    style->inheritFrom(renderStyle());
    child->setStyle(style.release());
}

void PseudoElement::didRecalcStyle(StyleChange)
{
    if (!renderer())
        return;

    // The renderers inside pseudo elements are anonymous so they don't get notified of recalcStyle and must have
    // the stylr propagated downward manually similar to RenderObject::propagateStyleToAnonymousChildren.
    RenderObject* renderer = this->renderer();
    for (RenderObject* child = renderer->nextInPreOrder(renderer); child; child = child->nextInPreOrder(renderer))
        updateChildStyle(child);
}

RenderObject* PseudoElement::createRendererForContent(const ContentData* content) const
{
    RenderArena* arena = this->renderer()->renderArena();
    RenderObject* renderer = 0;

    switch (content->type()) {
    case CONTENT_NONE:
        ASSERT_NOT_REACHED();
        return 0;
    case CONTENT_TEXT:
        renderer = new (arena) RenderTextFragment(document(), static_cast<const TextContentData*>(content)->text().impl());
        break;
    case CONTENT_OBJECT: {
        RenderImage* image = new (arena) RenderImage(document());
        // FIXME: This might be wrong since it causes a style change. See RenderObject::createObject.
        updateChildStyle(image);
        if (const StyleImage* styleImage = static_cast<const ImageContentData*>(content)->image())
            image->setImageResource(RenderImageResourceStyleImage::create(const_cast<StyleImage*>(styleImage)));
        else
            image->setImageResource(RenderImageResource::create());
        return image;
    }
    case CONTENT_COUNTER:
        renderer = new (arena) RenderCounter(document(), *static_cast<const CounterContentData*>(content)->counter());
        break;
    case CONTENT_QUOTE:
        renderer = new (arena) RenderQuote(document(), static_cast<const QuoteContentData*>(content)->quote());
        break;
    }

    ASSERT(renderer);
    updateChildStyle(renderer);

    return renderer;
}

} // namespace
