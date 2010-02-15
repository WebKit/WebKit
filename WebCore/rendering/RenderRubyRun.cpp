/*
 * Copyright (C) 2009 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
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

#if ENABLE(RUBY)
#include "RenderRubyRun.h"

#include "RenderRubyBase.h"
#include "RenderRubyText.h"
#include "RenderView.h"

using namespace std;

namespace WebCore {

RenderRubyRun::RenderRubyRun(Node* node)
    : RenderBlock(node)
    , m_beingDestroyed(false)
{
    setReplaced(true);
    setInline(true);
}

RenderRubyRun::~RenderRubyRun()
{
}

void RenderRubyRun::destroy()
{
    // Mark if the run is being destroyed to avoid trouble in removeChild().
    m_beingDestroyed = true;
    RenderBlock::destroy();
}

bool RenderRubyRun::hasRubyText() const
{
    // The only place where a ruby text can be is in the first position
    // Note: As anonymous blocks, ruby runs do not have ':before' or ':after' content themselves.
    return firstChild() && firstChild()->isRubyText();
}

bool RenderRubyRun::hasRubyBase() const
{
    // The only place where a ruby base can be is in the last position
    // Note: As anonymous blocks, ruby runs do not have ':before' or ':after' content themselves.
    return lastChild() && lastChild()->isRubyBase();
}

bool RenderRubyRun::isEmpty() const
{
    return !hasRubyText() && !hasRubyBase();
}

RenderRubyText* RenderRubyRun::rubyText() const
{
    RenderObject* child = firstChild();
    return child && child->isRubyText() ? static_cast<RenderRubyText*>(child) : 0;
}

RenderRubyBase* RenderRubyRun::rubyBase() const
{
    RenderObject* child = lastChild();
    return child && child->isRubyBase() ? static_cast<RenderRubyBase*>(child) : 0;
}

RenderRubyBase* RenderRubyRun::rubyBaseSafe()
{
    RenderRubyBase* base = rubyBase();
    if (!base) {
        base = createRubyBase();
        RenderBlock::addChild(base);
    }
    return base;
}

RenderBlock* RenderRubyRun::firstLineBlock() const
{
    return 0;
}

void RenderRubyRun::updateFirstLetter()
{
}

bool RenderRubyRun::isChildAllowed(RenderObject* child, RenderStyle*) const
{
    return child->isRubyText() || child->isInline();
}

void RenderRubyRun::addChild(RenderObject* child, RenderObject* beforeChild)
{
    ASSERT(child);

    // If child is a ruby text
    if (child->isRubyText()) {
        if (!beforeChild) {
            // RenderRuby has already ascertained that we can add the child here.
            ASSERT(!hasRubyText());
            // prepend ruby texts as first child
            RenderBlock::addChild(child, firstChild());
        }  else if (beforeChild->isRubyText()) {
            // New text is inserted just before another.
            // In this case the new text takes the place of the old one, and
            // the old text goes into a new run that is inserted as next sibling.
            ASSERT(beforeChild->parent() == this);
            RenderObject* ruby = parent();
            ASSERT(ruby->isRuby());
            RenderBlock* newRun = staticCreateRubyRun(ruby);
            ruby->addChild(newRun, nextSibling());
            // Add the new ruby text and move the old one to the new run
            // Note: Doing it in this order and not using RenderRubyRun's methods,
            // in order to avoid automatic removal of the ruby run in case there is no
            // other child besides the old ruby text.
            RenderBlock::addChild(child, beforeChild);
            RenderBlock::removeChild(beforeChild);
            newRun->addChild(beforeChild);
        } else {
            ASSERT(hasRubyBase()); // Otherwise beforeChild would be borked.
            // Insertion before a ruby base object.
            // In this case we need insert a new run before the current one and split the base.
            RenderObject* ruby = parent();
            RenderRubyRun* newRun = staticCreateRubyRun(ruby);
            ruby->addChild(newRun, this);
            newRun->addChild(child);
            rubyBaseSafe()->moveChildren(newRun->rubyBaseSafe(), beforeChild);
        }
    } else {
        // child is not a text -> insert it into the base
        // (append it instead if beforeChild is the ruby text)
        if (beforeChild && beforeChild->isRubyText())
            beforeChild = 0;
        rubyBaseSafe()->addChild(child, beforeChild);
    }
}

void RenderRubyRun::removeChild(RenderObject* child)
{
    // If the child is a ruby text, then merge the ruby base with the base of
    // the right sibling run, if possible.
    if (!m_beingDestroyed && !documentBeingDestroyed() && child->isRubyText()) {
        RenderRubyBase* base = rubyBase();
        RenderObject* rightNeighbour = nextSibling();
        if (base && rightNeighbour && rightNeighbour->isRubyRun()) {
            // Ruby run without a base can happen only at the first run.
            RenderRubyRun* rightRun = static_cast<RenderRubyRun*>(rightNeighbour);
            ASSERT(rightRun->hasRubyBase());
            RenderRubyBase* rightBase = rightRun->rubyBaseSafe();
            // Collect all children in a single base, then swap the bases.
            rightBase->moveChildren(base);
            moveChildTo(rightRun, rightRun->children(), base);
            rightRun->moveChildTo(this, children(), rightBase);
            // The now empty ruby base will be removed below.
        }
    }

    RenderBlock::removeChild(child);

    if (!m_beingDestroyed && !documentBeingDestroyed()) {
        // Check if our base (if any) is now empty. If so, destroy it.
        RenderBlock* base = rubyBase();
        if (base && !base->firstChild()) {
            RenderBlock::removeChild(base);
            base->deleteLineBoxTree();
            base->destroy();
        }

        // If any of the above leaves the run empty, destroy it as well.
        if (isEmpty()) {
            parent()->removeChild(this);
            deleteLineBoxTree();
            destroy();
        }
    }
}

RenderRubyBase* RenderRubyRun::createRubyBase() const
{
    RenderRubyBase* rb = new (renderArena()) RenderRubyBase(document() /* anonymous */);
    RefPtr<RenderStyle> newStyle = RenderStyle::create();
    newStyle->inheritFrom(style());
    newStyle->setDisplay(BLOCK);
    newStyle->setTextAlign(CENTER); // FIXME: use WEBKIT_CENTER?
    rb->setStyle(newStyle.release());
    return rb;
}

RenderRubyRun* RenderRubyRun::staticCreateRubyRun(const RenderObject* parentRuby)
{
    ASSERT(parentRuby && parentRuby->isRuby());
    RenderRubyRun* rr = new (parentRuby->renderArena()) RenderRubyRun(parentRuby->document() /* anonymous */);
    RefPtr<RenderStyle> newStyle = RenderStyle::create();
    newStyle->inheritFrom(parentRuby->style());
    newStyle->setDisplay(INLINE_BLOCK);
    rr->setStyle(newStyle.release());
    return rr;
}

} // namespace WebCore

#endif // ENABLE(RUBY)
