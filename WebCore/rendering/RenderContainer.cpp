/**
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2000 Dirk Mueller (mueller@kde.org)
 *           (C) 2004 Allan Sandfeld Jensen (kde@carewolf.com)
 * Copyright (C) 2003, 2004, 2005, 2006, 2007 Apple Inc. All rights reserved.
 * Copyright (C) 2006 Andrew Wellington (proton@wiretapped.net)
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 */

#include "config.h"
#include "RenderContainer.h"

#include "AXObjectCache.h"
#include "Document.h"
#include "RenderCounter.h"
#include "RenderImageGeneratedContent.h"
#include "RenderInline.h"
#include "RenderLayer.h"
#include "RenderListItem.h"
#include "RenderTable.h"
#include "RenderTextFragment.h"
#include "RenderView.h"
#include "htmlediting.h"

namespace WebCore {

RenderContainer::RenderContainer(Node* node)
    : RenderBox(node)
{
}

RenderContainer::~RenderContainer()
{
}

void RenderContainer::destroy()
{
    destroyLeftoverChildren();
    RenderBox::destroy();
}

void RenderContainer::destroyLeftoverChildren()
{
    while (m_children.firstChild()) {
        if (m_children.firstChild()->isListMarker() || (m_children.firstChild()->style()->styleType() == RenderStyle::FIRST_LETTER && !m_children.firstChild()->isText()))
            m_children.firstChild()->remove();  // List markers are owned by their enclosing list and so don't get destroyed by this container. Similarly, first letters are destroyed by their remaining text fragment.
        else {
        // Destroy any anonymous children remaining in the render tree, as well as implicit (shadow) DOM elements like those used in the engine-based text fields.
            if (m_children.firstChild()->element())
                m_children.firstChild()->element()->setRenderer(0);
            m_children.firstChild()->destroy();
        }
    }
}

bool RenderContainer::canHaveChildren() const
{
    return true;
}

static void updateListMarkerNumbers(RenderObject* child)
{
    for (RenderObject* r = child; r; r = r->nextSibling())
        if (r->isListItem())
            static_cast<RenderListItem*>(r)->updateValue();
}

void RenderContainer::addChild(RenderObject* newChild, RenderObject* beforeChild)
{
    bool needsTable = false;

    if (newChild->isListItem())
        updateListMarkerNumbers(beforeChild ? beforeChild : m_children.lastChild());
    else if (newChild->isTableCol() && newChild->style()->display() == TABLE_COLUMN_GROUP)
        needsTable = !isTable();
    else if (newChild->isRenderBlock() && newChild->style()->display() == TABLE_CAPTION)
        needsTable = !isTable();
    else if (newChild->isTableSection())
        needsTable = !isTable();
    else if (newChild->isTableRow())
        needsTable = !isTableSection();
    else if (newChild->isTableCell()) {
        needsTable = !isTableRow();
        // I'm not 100% sure this is the best way to fix this, but without this
        // change we recurse infinitely when trying to render the CSS2 test page:
        // http://www.bath.ac.uk/%7Epy8ieh/internet/eviltests/htmlbodyheadrendering2.html.
        // See Radar 2925291.
        if (needsTable && isTableCell() && !m_children.firstChild() && !newChild->isTableCell())
            needsTable = false;
    }

    if (needsTable) {
        RenderTable* table;
        RenderObject* afterChild = beforeChild ? beforeChild->previousSibling() : m_children.lastChild();
        if (afterChild && afterChild->isAnonymous() && afterChild->isTable())
            table = static_cast<RenderTable*>(afterChild);
        else {
            table = new (renderArena()) RenderTable(document() /* is anonymous */);
            RefPtr<RenderStyle> newStyle = RenderStyle::create();
            newStyle->inheritFrom(style());
            newStyle->setDisplay(TABLE);
            table->setStyle(newStyle.release());
            addChild(table, beforeChild);
        }
        table->addChild(newChild);
    } else {
        // just add it...
        insertChildNode(newChild, beforeChild);
    }
    
    if (newChild->isText() && newChild->style()->textTransform() == CAPITALIZE) {
        RefPtr<StringImpl> textToTransform = toRenderText(newChild)->originalText();
        if (textToTransform)
            toRenderText(newChild)->setText(textToTransform.release(), true);
    }
}

RenderObject* RenderContainer::removeChildNode(RenderObject* oldChild, bool fullRemove)
{
    ASSERT(oldChild->parent() == this);

    // So that we'll get the appropriate dirty bit set (either that a normal flow child got yanked or
    // that a positioned child got yanked).  We also repaint, so that the area exposed when the child
    // disappears gets repainted properly.
    if (!documentBeingDestroyed() && fullRemove && oldChild->m_everHadLayout) {
        oldChild->setNeedsLayoutAndPrefWidthsRecalc();
        oldChild->repaint();
    }
        
    // If we have a line box wrapper, delete it.
    oldChild->deleteLineBoxWrapper();

    if (!documentBeingDestroyed() && fullRemove) {
        // if we remove visible child from an invisible parent, we don't know the layer visibility any more
        RenderLayer* layer = 0;
        if (m_style->visibility() != VISIBLE && oldChild->style()->visibility() == VISIBLE && !oldChild->hasLayer()) {
            layer = enclosingLayer();
            layer->dirtyVisibleContentStatus();
        }

         // Keep our layer hierarchy updated.
        if (oldChild->firstChild() || oldChild->hasLayer()) {
            if (!layer) layer = enclosingLayer();            
            oldChild->removeLayers(layer);
        }
        
        // renumber ordered lists
        if (oldChild->isListItem())
            updateListMarkerNumbers(oldChild->nextSibling());
        
        if (oldChild->isPositioned() && childrenInline())
            dirtyLinesFromChangedChild(oldChild);
    }
    
    // If oldChild is the start or end of the selection, then clear the selection to
    // avoid problems of invalid pointers.
    // FIXME: The SelectionController should be responsible for this when it
    // is notified of DOM mutations.
    if (!documentBeingDestroyed() && oldChild->isSelectionBorder())
        view()->clearSelection();

    // remove the child
    if (oldChild->previousSibling())
        oldChild->previousSibling()->setNextSibling(oldChild->nextSibling());
    if (oldChild->nextSibling())
        oldChild->nextSibling()->setPreviousSibling(oldChild->previousSibling());

    if (m_children.firstChild() == oldChild)
        m_children.setFirstChild(oldChild->nextSibling());
    if (m_children.lastChild() == oldChild)
        m_children.setLastChild(oldChild->previousSibling());

    oldChild->setPreviousSibling(0);
    oldChild->setNextSibling(0);
    oldChild->setParent(0);

    if (AXObjectCache::accessibilityEnabled())
        document()->axObjectCache()->childrenChanged(this);

    return oldChild;
}

void RenderContainer::removeChild(RenderObject* oldChild)
{
    // We do this here instead of in removeChildNode, since the only extremely low-level uses of remove/appendChildNode
    // cannot affect the positioned object list, and the floating object list is irrelevant (since the list gets cleared on
    // layout anyway).
    oldChild->removeFromObjectLists();
    
    removeChildNode(oldChild);
}

RenderObject* RenderContainer::beforeAfterContainer(RenderStyle::PseudoId type)
{
    if (type == RenderStyle::BEFORE) {
        RenderObject* first = this;
        do {
            // Skip list markers.
            first = first->firstChild();
            while (first && first->isListMarker())
                first = first->nextSibling();
        } while (first && first->isAnonymous() && first->style()->styleType() == RenderStyle::NOPSEUDO);
        if (first && first->style()->styleType() != type)
            return 0;
        return first;
    }
    if (type == RenderStyle::AFTER) {
        RenderObject* last = this;
        do {
            last = last->lastChild();
        } while (last && last->isAnonymous() && last->style()->styleType() == RenderStyle::NOPSEUDO && !last->isListMarker());
        if (last && last->style()->styleType() != type)
            return 0;
        return last;
    }

    ASSERT_NOT_REACHED();
    return 0;
}

void RenderContainer::updateBeforeAfterContent(RenderStyle::PseudoId type)
{
    // If this is an anonymous wrapper, then the parent applies its own pseudo-element style to it.
    if (parent() && parent()->createsAnonymousWrapper())
        return;
    updateBeforeAfterContentForContainer(type, this);
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

void RenderContainer::updateBeforeAfterContentForContainer(RenderStyle::PseudoId type, RenderContainer* styledObject)
{
    // Double check that the document did in fact use generated content rules.  Otherwise we should not have been called.
    ASSERT(document()->usesBeforeAfterRules());

    // In CSS2, before/after pseudo-content cannot nest.  Check this first.
    if (style()->styleType() == RenderStyle::BEFORE || style()->styleType() == RenderStyle::AFTER)
        return;
    
    RenderStyle* pseudoElementStyle = styledObject->getCachedPseudoStyle(type);
    RenderObject* child = beforeAfterContainer(type);

    // Whether or not we currently have generated content attached.
    bool oldContentPresent = child;

    // Whether or not we now want generated content.  
    bool newContentWanted = pseudoElementStyle && pseudoElementStyle->display() != NONE;

    // For <q><p/></q>, if this object is the inline continuation of the <q>, we only want to generate
    // :after content and not :before content.
    if (newContentWanted && type == RenderStyle::BEFORE && isRenderInline() && toRenderInline(this)->isInlineContinuation())
        newContentWanted = false;

    // Similarly, if we're the beginning of a <q>, and there's an inline continuation for our object,
    // then we don't generate the :after content.
    if (newContentWanted && type == RenderStyle::AFTER && isRenderInline() && toRenderInline(this)->continuation())
        newContentWanted = false;
    
    // If we don't want generated content any longer, or if we have generated content, but it's no longer
    // identical to the new content data we want to build render objects for, then we nuke all
    // of the old generated content.
    if (!newContentWanted || (oldContentPresent && Node::diff(child->style(), pseudoElementStyle) == Node::Detach)) {
        // Nuke the child. 
        if (child && child->style()->styleType() == type) {
            oldContentPresent = false;
            child->destroy();
            child = (type == RenderStyle::BEFORE) ? m_children.firstChild() : m_children.lastChild();
        }
    }

    // If we have no pseudo-element style or if the pseudo-element style's display type is NONE, then we
    // have no generated content and can now return.
    if (!newContentWanted)
        return;

    if (isRenderInline() && !pseudoElementStyle->isDisplayInlineType() && pseudoElementStyle->floating() == FNONE &&
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
                    ASSERT(genChild->style()->styleType() == RenderStyle::FIRST_LETTER);
            }
        }
        return; // We've updated the generated content. That's all we needed to do.
    }
    
    RenderObject* insertBefore = (type == RenderStyle::BEFORE) ? m_children.firstChild() : 0;

    // Generated content consists of a single container that houses multiple children (specified
    // by the content property).  This generated content container gets the pseudo-element style set on it.
    RenderObject* generatedContentContainer = 0;
    
    // Walk our list of generated content and create render objects for each.
    for (const ContentData* content = pseudoElementStyle->contentData(); content; content = content->m_next) {
        RenderObject* renderer = 0;
        switch (content->m_type) {
            case CONTENT_NONE:
                break;
            case CONTENT_TEXT:
                renderer = new (renderArena()) RenderTextFragment(document() /* anonymous object */, content->m_content.m_text);
                renderer->setStyle(pseudoElementStyle);
                break;
            case CONTENT_OBJECT: {
                RenderImageGeneratedContent* image = new (renderArena()) RenderImageGeneratedContent(document()); // anonymous object
                RefPtr<RenderStyle> style = RenderStyle::create();
                style->inheritFrom(pseudoElementStyle);
                image->setStyle(style.release());
                if (StyleImage* styleImage = content->m_content.m_image)
                    image->setStyleImage(styleImage);
                renderer = image;
                break;
            }
            case CONTENT_COUNTER:
                renderer = new (renderArena()) RenderCounter(document(), *content->m_content.m_counter);
                renderer->setStyle(pseudoElementStyle);
                break;
        }

        if (renderer) {
            if (!generatedContentContainer) {
                // Make a generated box that might be any display type now that we are able to drill down into children
                // to find the original content properly.
                generatedContentContainer = RenderObject::createObject(document(), pseudoElementStyle);
                generatedContentContainer->setStyle(pseudoElementStyle);
                addChild(generatedContentContainer, insertBefore);
            }
            generatedContentContainer->addChild(renderer);
        }
    }
}

bool RenderContainer::isAfterContent(RenderObject* child) const
{
    if (!child)
        return false;
    if (child->style()->styleType() != RenderStyle::AFTER)
        return false;
    // Text nodes don't have their own styles, so ignore the style on a text node.
    if (child->isText() && !child->isBR())
        return false;
    return true;
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

void RenderContainer::invalidateCounters()
{
    if (documentBeingDestroyed())
        return;

    invalidateCountersInContainer(beforeAfterContainer(RenderStyle::BEFORE));
    invalidateCountersInContainer(beforeAfterContainer(RenderStyle::AFTER));
}

void RenderContainer::appendChildNode(RenderObject* newChild, bool fullAppend)
{
    ASSERT(newChild->parent() == 0);
    ASSERT(!isBlockFlow() || (!newChild->isTableSection() && !newChild->isTableRow() && !newChild->isTableCell()));

    newChild->setParent(this);
    RenderObject* lChild = m_children.lastChild();

    if (lChild) {
        newChild->setPreviousSibling(lChild);
        lChild->setNextSibling(newChild);
    } else
        m_children.setFirstChild(newChild);

    m_children.setLastChild(newChild);
    
    if (fullAppend) {
        // Keep our layer hierarchy updated.  Optimize for the common case where we don't have any children
        // and don't have a layer attached to ourselves.
        RenderLayer* layer = 0;
        if (newChild->firstChild() || newChild->hasLayer()) {
            layer = enclosingLayer();
            newChild->addLayers(layer, newChild);
        }

        // if the new child is visible but this object was not, tell the layer it has some visible content
        // that needs to be drawn and layer visibility optimization can't be used
        if (style()->visibility() != VISIBLE && newChild->style()->visibility() == VISIBLE && !newChild->hasLayer()) {
            if (!layer)
                layer = enclosingLayer();
            if (layer)
                layer->setHasVisibleContent(true);
        }
        
        if (!newChild->isFloatingOrPositioned() && childrenInline())
            dirtyLinesFromChangedChild(newChild);
    }

    newChild->setNeedsLayoutAndPrefWidthsRecalc(); // Goes up the containing block hierarchy.
    if (!normalChildNeedsLayout())
        setChildNeedsLayout(true); // We may supply the static position for an absolute positioned child.
    
    if (AXObjectCache::accessibilityEnabled())
        document()->axObjectCache()->childrenChanged(this);
}

void RenderContainer::insertChildNode(RenderObject* child, RenderObject* beforeChild, bool fullInsert)
{
    if (!beforeChild) {
        appendChildNode(child);
        return;
    }

    ASSERT(!child->parent());
    while (beforeChild->parent() != this && beforeChild->parent()->isAnonymousBlock())
        beforeChild = beforeChild->parent();
    ASSERT(beforeChild->parent() == this);

    ASSERT(!isBlockFlow() || (!child->isTableSection() && !child->isTableRow() && !child->isTableCell()));

    if (beforeChild == m_children.firstChild())
        m_children.setFirstChild(child);

    RenderObject* prev = beforeChild->previousSibling();
    child->setNextSibling(beforeChild);
    beforeChild->setPreviousSibling(child);
    if(prev) prev->setNextSibling(child);
    child->setPreviousSibling(prev);

    child->setParent(this);
    
    if (fullInsert) {
        // Keep our layer hierarchy updated.  Optimize for the common case where we don't have any children
        // and don't have a layer attached to ourselves.
        RenderLayer* layer = 0;
        if (child->firstChild() || child->hasLayer()) {
            layer = enclosingLayer();
            child->addLayers(layer, child);
        }

        // if the new child is visible but this object was not, tell the layer it has some visible content
        // that needs to be drawn and layer visibility optimization can't be used
        if (style()->visibility() != VISIBLE && child->style()->visibility() == VISIBLE && !child->hasLayer()) {
            if (!layer)
                layer = enclosingLayer();
            if (layer)
                layer->setHasVisibleContent(true);
        }

        
        if (!child->isFloating() && childrenInline())
            dirtyLinesFromChangedChild(child);
    }

    child->setNeedsLayoutAndPrefWidthsRecalc();
    if (!normalChildNeedsLayout())
        setChildNeedsLayout(true); // We may supply the static position for an absolute positioned child.
    
    if (AXObjectCache::accessibilityEnabled())
        document()->axObjectCache()->childrenChanged(this);
}

void RenderContainer::layout()
{
    ASSERT(needsLayout());

    LayoutStateMaintainer statePusher(view(), this, IntSize(x(), y()));

    RenderObject* child = m_children.firstChild();
    while (child) {
        child->layoutIfNeeded();
        ASSERT(child->isRenderInline() || !child->needsLayout());
        child = child->nextSibling();
    }

    statePusher.pop();
    setNeedsLayout(false);
}

void RenderContainer::removeLeftoverAnonymousBlock(RenderBlock* child)
{
    ASSERT(child->isAnonymousBlock());
    ASSERT(!child->childrenInline());
    
    if (child->inlineContinuation()) 
        return;
    
    RenderObject* firstAnChild = child->m_children.firstChild();
    RenderObject* lastAnChild = child->m_children.lastChild();
    if (firstAnChild) {
        RenderObject* o = firstAnChild;
        while(o) {
            o->setParent(this);
            o = o->nextSibling();
        }
        firstAnChild->setPreviousSibling(child->previousSibling());
        lastAnChild->setNextSibling(child->nextSibling());
        if (child->previousSibling())
            child->previousSibling()->setNextSibling(firstAnChild);
        if (child->nextSibling())
            child->nextSibling()->setPreviousSibling(lastAnChild);
    } else {
        if (child->previousSibling())
            child->previousSibling()->setNextSibling(child->nextSibling());
        if (child->nextSibling())
            child->nextSibling()->setPreviousSibling(child->previousSibling());
    }
    if (child == m_children.firstChild())
        m_children.setFirstChild(firstAnChild);
    if (child == m_children.lastChild())
        m_children.setLastChild(lastAnChild);
    child->setParent(0);
    child->setPreviousSibling(0);
    child->setNextSibling(0);
    if (!child->isText()) {
        RenderContainer* c = static_cast<RenderContainer*>(child);
        c->m_children.setFirstChild(0);
        c->m_next = 0;
    }
    child->destroy();
}

VisiblePosition RenderContainer::positionForCoordinates(int xPos, int yPos)
{
    // no children...return this render object's element, if there is one, and offset 0
    if (!m_children.firstChild())
        return VisiblePosition(element(), 0, DOWNSTREAM);
        
    if (isTable() && element()) {
        int right = contentWidth() + borderRight() + paddingRight() + borderLeft() + paddingLeft();
        int bottom = contentHeight() + borderTop() + paddingTop() + borderBottom() + paddingBottom();
        
        if (xPos < 0 || xPos > right || yPos < 0 || yPos > bottom) {
            if (xPos <= right / 2)
                return VisiblePosition(Position(element(), 0));
            else
                return VisiblePosition(Position(element(), maxDeepOffset(element())));
        }
    }

    // Pass off to the closest child.
    int minDist = INT_MAX;
    RenderBox* closestRenderer = 0;
    int newX = xPos;
    int newY = yPos;
    if (isTableRow()) {
        newX += x();
        newY += y();
    }
    for (RenderObject* renderObject = m_children.firstChild(); renderObject; renderObject = renderObject->nextSibling()) {
        if (!renderObject->firstChild() && !renderObject->isInline() && !renderObject->isBlockFlow() 
            || renderObject->style()->visibility() != VISIBLE)
            continue;
        
        if (!renderObject->isBox())
            continue;
        
        RenderBox* renderer = toRenderBox(renderObject);

        int top = borderTop() + paddingTop() + (isTableRow() ? 0 : renderer->y());
        int bottom = top + renderer->contentHeight();
        int left = borderLeft() + paddingLeft() + (isTableRow() ? 0 : renderer->x());
        int right = left + renderer->contentWidth();
        
        if (xPos <= right && xPos >= left && yPos <= top && yPos >= bottom) {
            if (renderer->isTableRow())
                return renderer->positionForCoordinates(xPos + newX - renderer->x(), yPos + newY - renderer->y());
            return renderer->positionForCoordinates(xPos - renderer->x(), yPos - renderer->y());
        }

        // Find the distance from (x, y) to the box.  Split the space around the box into 8 pieces
        // and use a different compare depending on which piece (x, y) is in.
        IntPoint cmp;
        if (xPos > right) {
            if (yPos < top)
                cmp = IntPoint(right, top);
            else if (yPos > bottom)
                cmp = IntPoint(right, bottom);
            else
                cmp = IntPoint(right, yPos);
        } else if (xPos < left) {
            if (yPos < top)
                cmp = IntPoint(left, top);
            else if (yPos > bottom)
                cmp = IntPoint(left, bottom);
            else
                cmp = IntPoint(left, yPos);
        } else {
            if (yPos < top)
                cmp = IntPoint(xPos, top);
            else
                cmp = IntPoint(xPos, bottom);
        }
        
        int x1minusx2 = cmp.x() - xPos;
        int y1minusy2 = cmp.y() - yPos;
        
        int dist = x1minusx2 * x1minusx2 + y1minusy2 * y1minusy2;
        if (dist < minDist) {
            closestRenderer = renderer;
            minDist = dist;
        }
    }
    
    if (closestRenderer)
        return closestRenderer->positionForCoordinates(newX - closestRenderer->x(), newY - closestRenderer->y());
    
    return VisiblePosition(element(), 0, DOWNSTREAM);
}

void RenderContainer::addLineBoxRects(Vector<IntRect>& rects, unsigned start, unsigned end, bool)
{
    if (!m_children.firstChild() && (isInline() || isAnonymousBlock())) {
        FloatPoint absPos = localToAbsolute(FloatPoint());
        absoluteRects(rects, absPos.x(), absPos.y());
        return;
    }

    if (!m_children.firstChild())
        return;

    unsigned offset = start;
    for (RenderObject* child = childAt(start); child && offset < end; child = child->nextSibling(), ++offset) {
        if (child->isText() || child->isInline() || child->isAnonymousBlock()) {
            FloatPoint absPos = child->localToAbsolute(FloatPoint());
            child->absoluteRects(rects, absPos.x(), absPos.y());
        }
    }
}

void RenderContainer::collectAbsoluteLineBoxQuads(Vector<FloatQuad>& quads, unsigned start, unsigned end, bool /*useSelectionHeight*/)
{
    if (!m_children.firstChild() && (isInline() || isAnonymousBlock())) {
        absoluteQuads(quads);
        return;
    }

    if (!m_children.firstChild())
        return;

    unsigned offset = start;
    for (RenderObject* child = childAt(start); child && offset < end; child = child->nextSibling(), ++offset) {
        if (child->isText() || child->isInline() || child->isAnonymousBlock())
            child->absoluteQuads(quads);
    }
}

#undef DEBUG_LAYOUT

} // namespace WebCore
