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
#include "RenderRuby.h"

#include "RenderRubyRun.h"

namespace WebCore {

//=== generic helper functions to avoid excessive code duplication ===

static RenderRubyRun* lastRubyRun(const RenderObject* ruby)
{
    RenderObject* child = ruby->lastChild();
    if (child && ruby->isAfterContent(child))
        child = child->previousSibling();
    ASSERT(!child || child->isRubyRun());
    return static_cast<RenderRubyRun*>(child);
}

static inline RenderRubyRun* findRubyRunParent(RenderObject* child)
{
    while (child && !child->isRubyRun())
        child = child->parent();
    return static_cast<RenderRubyRun*>(child);
}

//=== ruby as inline object ===

RenderRubyAsInline::RenderRubyAsInline(Node* node)
    : RenderInline(node)
{
}

RenderRubyAsInline::~RenderRubyAsInline()
{
}

bool RenderRubyAsInline::isChildAllowed(RenderObject* child, RenderStyle*) const
{
    return child->isRubyText()
        || child->isRubyRun()
        || child->isInline();
}

void RenderRubyAsInline::addChild(RenderObject* child, RenderObject* beforeChild)
{
    // Note: ':after' content is handled implicitely below

    // if child is a ruby run, just add it normally
    if (child->isRubyRun()) {
        RenderInline::addChild(child, beforeChild);
        return;
    }

    if (beforeChild && !isAfterContent(beforeChild)) {
        // insert child into run
        ASSERT(!beforeChild->isRubyRun());
        RenderRubyRun* run = findRubyRunParent(beforeChild);
        ASSERT(run); // beforeChild should always have a run as parent
        if (run) {
            run->addChild(child, beforeChild);
            return;
        }
        ASSERT(false); // beforeChild should always have a run as parent!
        // Emergency fallback: fall through and just append.
    }

    // If the new child would be appended, try to add the child to the previous run
    // if possible, or create a new run otherwise.
    // (The RenderRubyRun object will handle the details)
    RenderRubyRun* lastRun = lastRubyRun(this);
    if (!lastRun || lastRun->hasRubyText()) {
        lastRun = RenderRubyRun::staticCreateRubyRun(this);
        RenderInline::addChild(lastRun);
    }
    lastRun->addChild(child);
}

void RenderRubyAsInline::removeChild(RenderObject* child)
{
    // If the child's parent is *this, i.e. a ruby run or ':after' content,
    // just use the normal remove method.
    if (child->parent() == this) {
        ASSERT(child->isRubyRun() || child->isAfterContent());
        RenderInline::removeChild(child);
        return;
    }

    // Find the containing run
    RenderRubyRun* run = findRubyRunParent(child);
    ASSERT(run);
    run->removeChild(child);
}


//=== ruby as block object ===

RenderRubyAsBlock::RenderRubyAsBlock(Node* node)
    : RenderBlock(node)
{
}

RenderRubyAsBlock::~RenderRubyAsBlock()
{
}

bool RenderRubyAsBlock::isChildAllowed(RenderObject* child, RenderStyle*) const
{
    return child->isRubyText()
        || child->isRubyRun()
        || child->isInline();
}

void RenderRubyAsBlock::addChild(RenderObject* child, RenderObject* beforeChild)
{
    // Note: ':after' content is handled implicitely below

    // if child is a ruby run, just add it normally
    if (child->isRubyRun()) {
        RenderBlock::addChild(child, beforeChild);
        return;
    }

    if (beforeChild && !isAfterContent(beforeChild)) {
        // insert child into run
        ASSERT(!beforeChild->isRubyRun());
        RenderObject* run = beforeChild;
        while (run && !run->isRubyRun())
            run = run->parent();
        if (run) {
            run->addChild(child, beforeChild);
            return;
        }
        ASSERT(false); // beforeChild should always have a run as parent!
        // Emergency fallback: fall through and just append.
    }

    // If the new child would be appended, try to add the child to the previous run
    // if possible, or create a new run otherwise.
    // (The RenderRubyRun object will handle the details)
    RenderRubyRun* lastRun = lastRubyRun(this);
    if (!lastRun || lastRun->hasRubyText()) {
        lastRun = RenderRubyRun::staticCreateRubyRun(this);
        RenderBlock::addChild(lastRun);
    }
    lastRun->addChild(child);
}

void RenderRubyAsBlock::removeChild(RenderObject* child)
{
    // If the child's parent is *this, just use the normal remove method.
    if (child->parent() == this) {
        // This should happen only during destruction of the whole ruby element, though.
        RenderBlock::removeChild(child);
        return;
    }

    // Find the containing run
    RenderRubyRun* run = findRubyRunParent(child);
    ASSERT(run);
    run->removeChild(child);
}

} // namespace WebCore

#endif
