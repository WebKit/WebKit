/*
 * Copyright (C) 2009 Apple Inc.  All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#include "config.h"
#include "RenderObjectChildList.h"

#include "AXObjectCache.h"
#include "RenderBlock.h"
#include "RenderCounter.h"
#include "RenderImageGeneratedContent.h"
#include "RenderInline.h"
#include "RenderLayer.h"
#include "RenderListItem.h"
#include "RenderStyle.h"
#include "RenderTextFragment.h"
#include "RenderView.h"

namespace WebCore {

static void updateListMarkerNumbers(RenderObject* child)
{
    for (RenderObject* r = child; r; r = r->nextSibling()) {
        if (r->isListItem())
            static_cast<RenderListItem*>(r)->updateValue();
    }
}


void RenderObjectChildList::destroyLeftoverChildren()
{
    while (firstChild()) {
        if (firstChild()->isListMarker() || (firstChild()->style()->styleType() == FIRST_LETTER && !firstChild()->isText()))
            firstChild()->remove();  // List markers are owned by their enclosing list and so don't get destroyed by this container. Similarly, first letters are destroyed by their remaining text fragment.
        else {
            // Destroy any anonymous children remaining in the render tree, as well as implicit (shadow) DOM elements like those used in the engine-based text fields.
            if (firstChild()->node())
                firstChild()->node()->setRenderer(0);
            firstChild()->destroy();
        }
    }
}

RenderObject* RenderObjectChildList::removeChildNode(RenderObject* owner, RenderObject* oldChild, bool fullRemove)
{
    ASSERT(oldChild->parent() == owner);

    // So that we'll get the appropriate dirty bit set (either that a normal flow child got yanked or
    // that a positioned child got yanked).  We also repaint, so that the area exposed when the child
    // disappears gets repainted properly.
    if (!owner->documentBeingDestroyed() && fullRemove && oldChild->m_everHadLayout) {
        oldChild->setNeedsLayoutAndPrefWidthsRecalc();
        oldChild->repaint();
    }
        
    // If we have a line box wrapper, delete it.
    if (oldChild->isBox())
        toRenderBox(oldChild)->deleteLineBoxWrapper();

    if (!owner->documentBeingDestroyed() && fullRemove) {
        // if we remove visible child from an invisible parent, we don't know the layer visibility any more
        RenderLayer* layer = 0;
        if (owner->style()->visibility() != VISIBLE && oldChild->style()->visibility() == VISIBLE && !oldChild->hasLayer()) {
            layer = owner->enclosingLayer();
            layer->dirtyVisibleContentStatus();
        }

         // Keep our layer hierarchy updated.
        if (oldChild->firstChild() || oldChild->hasLayer()) {
            if (!layer)
                layer = owner->enclosingLayer();
            oldChild->removeLayers(layer);
        }
        
        // renumber ordered lists
        if (oldChild->isListItem())
            updateListMarkerNumbers(oldChild->nextSibling());
        
        if (oldChild->isPositioned() && owner->childrenInline())
            owner->dirtyLinesFromChangedChild(oldChild);
    }
    
    // If oldChild is the start or end of the selection, then clear the selection to
    // avoid problems of invalid pointers.
    // FIXME: The SelectionController should be responsible for this when it
    // is notified of DOM mutations.
    if (!owner->documentBeingDestroyed() && oldChild->isSelectionBorder())
        owner->view()->clearSelection();

    // remove the child
    if (oldChild->previousSibling())
        oldChild->previousSibling()->setNextSibling(oldChild->nextSibling());
    if (oldChild->nextSibling())
        oldChild->nextSibling()->setPreviousSibling(oldChild->previousSibling());

    if (firstChild() == oldChild)
        setFirstChild(oldChild->nextSibling());
    if (lastChild() == oldChild)
        setLastChild(oldChild->previousSibling());

    oldChild->setPreviousSibling(0);
    oldChild->setNextSibling(0);
    oldChild->setParent(0);

    if (AXObjectCache::accessibilityEnabled())
        owner->document()->axObjectCache()->childrenChanged(owner);

    return oldChild;
}

void RenderObjectChildList::appendChildNode(RenderObject* owner, RenderObject* newChild, bool fullAppend)
{
    ASSERT(newChild->parent() == 0);
    ASSERT(!owner->isBlockFlow() || (!newChild->isTableSection() && !newChild->isTableRow() && !newChild->isTableCell()));

    newChild->setParent(owner);
    RenderObject* lChild = lastChild();

    if (lChild) {
        newChild->setPreviousSibling(lChild);
        lChild->setNextSibling(newChild);
    } else
        setFirstChild(newChild);

    setLastChild(newChild);
    
    if (fullAppend) {
        // Keep our layer hierarchy updated.  Optimize for the common case where we don't have any children
        // and don't have a layer attached to ourselves.
        RenderLayer* layer = 0;
        if (newChild->firstChild() || newChild->hasLayer()) {
            layer = owner->enclosingLayer();
            newChild->addLayers(layer, newChild);
        }

        // if the new child is visible but this object was not, tell the layer it has some visible content
        // that needs to be drawn and layer visibility optimization can't be used
        if (owner->style()->visibility() != VISIBLE && newChild->style()->visibility() == VISIBLE && !newChild->hasLayer()) {
            if (!layer)
                layer = owner->enclosingLayer();
            if (layer)
                layer->setHasVisibleContent(true);
        }
        
        if (!newChild->isFloatingOrPositioned() && owner->childrenInline())
            owner->dirtyLinesFromChangedChild(newChild);
    }

    newChild->setNeedsLayoutAndPrefWidthsRecalc(); // Goes up the containing block hierarchy.
    if (!owner->normalChildNeedsLayout())
        owner->setChildNeedsLayout(true); // We may supply the static position for an absolute positioned child.
    
    if (AXObjectCache::accessibilityEnabled())
        owner->document()->axObjectCache()->childrenChanged(owner);
}

void RenderObjectChildList::insertChildNode(RenderObject* owner, RenderObject* child, RenderObject* beforeChild, bool fullInsert)
{
    if (!beforeChild) {
        appendChildNode(owner, child);
        return;
    }

    ASSERT(!child->parent());
    while (beforeChild->parent() != owner && beforeChild->parent()->isAnonymousBlock())
        beforeChild = beforeChild->parent();
    ASSERT(beforeChild->parent() == owner);

    ASSERT(!owner->isBlockFlow() || (!child->isTableSection() && !child->isTableRow() && !child->isTableCell()));

    if (beforeChild == firstChild())
        setFirstChild(child);

    RenderObject* prev = beforeChild->previousSibling();
    child->setNextSibling(beforeChild);
    beforeChild->setPreviousSibling(child);
    if (prev)
        prev->setNextSibling(child);
    child->setPreviousSibling(prev);

    child->setParent(owner);
    
    if (fullInsert) {
        // Keep our layer hierarchy updated.  Optimize for the common case where we don't have any children
        // and don't have a layer attached to ourselves.
        RenderLayer* layer = 0;
        if (child->firstChild() || child->hasLayer()) {
            layer = owner->enclosingLayer();
            child->addLayers(layer, child);
        }

        // if the new child is visible but this object was not, tell the layer it has some visible content
        // that needs to be drawn and layer visibility optimization can't be used
        if (owner->style()->visibility() != VISIBLE && child->style()->visibility() == VISIBLE && !child->hasLayer()) {
            if (!layer)
                layer = owner->enclosingLayer();
            if (layer)
                layer->setHasVisibleContent(true);
        }

        
        if (!child->isFloating() && owner->childrenInline())
            owner->dirtyLinesFromChangedChild(child);
    }

    child->setNeedsLayoutAndPrefWidthsRecalc();
    if (!owner->normalChildNeedsLayout())
        owner->setChildNeedsLayout(true); // We may supply the static position for an absolute positioned child.
    
    if (AXObjectCache::accessibilityEnabled())
        owner->document()->axObjectCache()->childrenChanged(owner);
}

static RenderObject* beforeAfterContainer(RenderObject* container, PseudoId type)
{
    if (type == BEFORE) {
        RenderObject* first = container;
        do {
            // Skip list markers.
            first = first->firstChild();
            while (first && first->isListMarker())
                first = first->nextSibling();
        } while (first && first->isAnonymous() && first->style()->styleType() == NOPSEUDO);
        if (first && first->style()->styleType() != type)
            return 0;
        return first;
    }
    if (type == AFTER) {
        RenderObject* last = container;
        do {
            last = last->lastChild();
        } while (last && last->isAnonymous() && last->style()->styleType() == NOPSEUDO && !last->isListMarker());
        if (last && last->style()->styleType() != type)
            return 0;
        return last;
    }

    ASSERT_NOT_REACHED();
    return 0;
}

static RenderObject* findBeforeAfterParent(RenderObject* object)
{
    // Only table parts need to search for the :before or :after parent
    if (!(object->isTable() || object->isTableSection() || object->isTableRow()))
        return object;

    RenderObject* beforeAfterParent = object;
    while (beforeAfterParent && !(beforeAfterParent->isText() || beforeAfterParent->isImage()))
        beforeAfterParent = beforeAfterParent->firstChild();
    return beforeAfterParent;
}

static void invalidateCountersInContainer(RenderObject* container)
{
    if (!container)
        return;
    container = findBeforeAfterParent(container);
    if (!container)
        return;
    for (RenderObject* content = container->firstChild(); content; content = content->nextSibling()) {
        if (content->isCounter())
            static_cast<RenderCounter*>(content)->invalidate();
    }
}

void RenderObjectChildList::invalidateCounters(RenderObject* owner)
{
    ASSERT(!owner->documentBeingDestroyed());
    invalidateCountersInContainer(beforeAfterContainer(owner, BEFORE));
    invalidateCountersInContainer(beforeAfterContainer(owner, AFTER));
}

void RenderObjectChildList::updateBeforeAfterContent(RenderObject* owner, PseudoId type, RenderObject* styledObject)
{
    // Double check that the document did in fact use generated content rules.  Otherwise we should not have been called.
    ASSERT(owner->document()->usesBeforeAfterRules());

    // In CSS2, before/after pseudo-content cannot nest.  Check this first.
    if (owner->style()->styleType() == BEFORE || owner->style()->styleType() == AFTER)
        return;
    
    if (!styledObject)
        styledObject = owner;

    RenderStyle* pseudoElementStyle = styledObject->getCachedPseudoStyle(type);
    RenderObject* child = beforeAfterContainer(owner, type);

    // Whether or not we currently have generated content attached.
    bool oldContentPresent = child;

    // Whether or not we now want generated content.  
    bool newContentWanted = pseudoElementStyle && pseudoElementStyle->display() != NONE;

    // For <q><p/></q>, if this object is the inline continuation of the <q>, we only want to generate
    // :after content and not :before content.
    if (newContentWanted && type == BEFORE && owner->isRenderInline() && toRenderInline(owner)->isInlineContinuation())
        newContentWanted = false;

    // Similarly, if we're the beginning of a <q>, and there's an inline continuation for our object,
    // then we don't generate the :after content.
    if (newContentWanted && type == AFTER && owner->isRenderInline() && toRenderInline(owner)->continuation())
        newContentWanted = false;
    
    // If we don't want generated content any longer, or if we have generated content, but it's no longer
    // identical to the new content data we want to build render objects for, then we nuke all
    // of the old generated content.
    if (!newContentWanted || (oldContentPresent && Node::diff(child->style(), pseudoElementStyle) == Node::Detach)) {
        // Nuke the child. 
        if (child && child->style()->styleType() == type) {
            oldContentPresent = false;
            child->destroy();
            child = (type == BEFORE) ? owner->virtualChildren()->firstChild() : owner->virtualChildren()->lastChild();
        }
    }

    // If we have no pseudo-element style or if the pseudo-element style's display type is NONE, then we
    // have no generated content and can now return.
    if (!newContentWanted)
        return;

    if (owner->isRenderInline() && !pseudoElementStyle->isDisplayInlineType() && pseudoElementStyle->floating() == FNONE &&
        !(pseudoElementStyle->position() == AbsolutePosition || pseudoElementStyle->position() == FixedPosition))
        // According to the CSS2 spec (the end of section 12.1), the only allowed
        // display values for the pseudo style are NONE and INLINE for inline flows.
        // FIXME: CSS2.1 lifted this restriction, but block display types will crash.
        // For now we at least relax the restriction to allow all inline types like inline-block
        // and inline-table.
        pseudoElementStyle->setDisplay(INLINE);

    if (oldContentPresent) {
        if (child && child->style()->styleType() == type) {
            // We have generated content present still.  We want to walk this content and update our
            // style information with the new pseudo-element style.
            child->setStyle(pseudoElementStyle);

            RenderObject* beforeAfterParent = findBeforeAfterParent(child);
            if (!beforeAfterParent)
                return;

            // Note that if we ever support additional types of generated content (which should be way off
            // in the future), this code will need to be patched.
            for (RenderObject* genChild = beforeAfterParent->firstChild(); genChild; genChild = genChild->nextSibling()) {
                if (genChild->isText())
                    // Generated text content is a child whose style also needs to be set to the pseudo-element style.
                    genChild->setStyle(pseudoElementStyle);
                else if (genChild->isImage()) {
                    // Images get an empty style that inherits from the pseudo.
                    RefPtr<RenderStyle> style = RenderStyle::create();
                    style->inheritFrom(pseudoElementStyle);
                    genChild->setStyle(style.release());
                } else
                    // Must be a first-letter container. updateFirstLetter() will take care of it.
                    ASSERT(genChild->style()->styleType() == FIRST_LETTER);
            }
        }
        return; // We've updated the generated content. That's all we needed to do.
    }
    
    RenderObject* insertBefore = (type == BEFORE) ? owner->virtualChildren()->firstChild() : 0;

    // Generated content consists of a single container that houses multiple children (specified
    // by the content property).  This generated content container gets the pseudo-element style set on it.
    RenderObject* generatedContentContainer = 0;
    
    // Walk our list of generated content and create render objects for each.
    for (const ContentData* content = pseudoElementStyle->contentData(); content; content = content->next()) {
        RenderObject* renderer = 0;
        switch (content->type()) {
            case CONTENT_NONE:
                break;
            case CONTENT_TEXT:
                renderer = new (owner->renderArena()) RenderTextFragment(owner->document() /* anonymous object */, content->text());
                renderer->setStyle(pseudoElementStyle);
                break;
            case CONTENT_OBJECT: {
                RenderImageGeneratedContent* image = new (owner->renderArena()) RenderImageGeneratedContent(owner->document()); // anonymous object
                RefPtr<RenderStyle> style = RenderStyle::create();
                style->inheritFrom(pseudoElementStyle);
                image->setStyle(style.release());
                if (StyleImage* styleImage = content->image())
                    image->setStyleImage(styleImage);
                renderer = image;
                break;
            }
            case CONTENT_COUNTER:
                renderer = new (owner->renderArena()) RenderCounter(owner->document(), *content->counter());
                renderer->setStyle(pseudoElementStyle);
                break;
        }

        if (renderer) {
            if (!generatedContentContainer) {
                // Make a generated box that might be any display type now that we are able to drill down into children
                // to find the original content properly.
                generatedContentContainer = RenderObject::createObject(owner->document(), pseudoElementStyle);
                generatedContentContainer->setStyle(pseudoElementStyle);
                owner->addChild(generatedContentContainer, insertBefore);
            }
            generatedContentContainer->addChild(renderer);
        }
    }
}

} // namespace WebCore
