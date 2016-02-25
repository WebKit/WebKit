/*
 * Copyright (C) 2015-2016 Apple Inc. All rights reserved.
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

#ifndef LargeObject_h
#define LargeObject_h

#include "BeginTag.h"
#include "EndTag.h"
#include "LargeChunk.h"
#include "Range.h"

namespace bmalloc {

class LargeObject {
public:
    static Range init(LargeChunk*);

    LargeObject();
    LargeObject(void*);

    enum DoNotValidateTag { DoNotValidate };
    LargeObject(DoNotValidateTag, void*);
    
    operator bool() { return !!*this; }
    bool operator!() { return !m_object; }

    char* begin() const { return static_cast<char*>(m_object); }
    char* end() const { return begin() + size(); }
    size_t size() const { return m_beginTag->size(); }
    Range range() const { return Range(m_object, size()); }

    void setFree(bool) const;
    bool isFree() const;

    bool prevCanMerge() const;
    bool nextCanMerge() const;

    VMState vmState() const;
    void setVMState(VMState) const;

    bool isMarked() const;
    void setMarked(bool) const;

    bool isValidAndFree(VMState::HasPhysical, size_t) const;

    LargeObject merge() const;
    std::pair<LargeObject, LargeObject> split(size_t) const;

private:
    LargeObject(BeginTag*, EndTag*, void*);

    void validate() const;
    void validateSelf() const;

    BeginTag* m_beginTag;
    EndTag* m_endTag;
    void* m_object;
};

inline LargeObject::LargeObject()
    : m_beginTag(nullptr)
    , m_endTag(nullptr)
    , m_object(nullptr)
{
}

inline LargeObject::LargeObject(void* object)
    : m_beginTag(LargeChunk::beginTag(object))
    , m_endTag(LargeChunk::endTag(object, m_beginTag->size()))
    , m_object(object)
{
    validate();
}

inline LargeObject::LargeObject(DoNotValidateTag, void* object)
    : m_beginTag(LargeChunk::beginTag(object))
    , m_endTag(LargeChunk::endTag(object, m_beginTag->size()))
    , m_object(object)
{
}

inline LargeObject::LargeObject(BeginTag* beginTag, EndTag* endTag, void* object)
    : m_beginTag(beginTag)
    , m_endTag(endTag)
    , m_object(object)
{
}

inline void LargeObject::setFree(bool isFree) const
{
    validate();
    m_beginTag->setFree(isFree);
    m_endTag->setFree(isFree);
}

inline bool LargeObject::isFree() const
{
    validate();
    return m_beginTag->isFree();
}

inline bool LargeObject::prevCanMerge() const
{
    return m_beginTag->prev()->isFree();
}

inline bool LargeObject::nextCanMerge() const
{
    return m_endTag->next()->isFree();
}

inline VMState LargeObject::vmState() const
{
    validate();
    return m_beginTag->vmState();
}

inline void LargeObject::setVMState(VMState vmState) const
{
    validate();
    m_beginTag->setVMState(vmState);
    m_endTag->setVMState(vmState);
}

inline bool LargeObject::isMarked() const
{
    validate();
    return m_beginTag->isMarked();
}

inline void LargeObject::setMarked(bool isMarked) const
{
    validate();
    m_beginTag->setMarked(isMarked);
    m_endTag->setMarked(isMarked);
}

inline bool LargeObject::isValidAndFree(VMState::HasPhysical hasPhysical, size_t expectedSize) const
{
    if (!m_beginTag->isFree())
        return false;
    
    if (m_beginTag->isEnd())
        return false;

    if (m_beginTag->size() != expectedSize)
        return false;
    
    if (m_beginTag->compactBegin() != BoundaryTag::compactBegin(m_object))
        return false;

    if (m_beginTag->vmState().hasPhysical() != static_cast<bool>(hasPhysical))
        return false;

    return true;
}

inline LargeObject LargeObject::merge() const
{
    validate();
    BASSERT(isFree());

    BeginTag* beginTag = m_beginTag;
    EndTag* endTag = m_endTag;
    Range range = this->range();
    VMState vmState = this->vmState();

    EndTag* prev = beginTag->prev();
    if (prev->isFree()) {
        vmState.merge(prev->vmState());
        Range left(range.begin() - prev->size(), prev->size());
        range = Range(left.begin(), left.size() + range.size());

        prev->clear();
        beginTag->clear();

        beginTag = LargeChunk::beginTag(range.begin());
    }

    BeginTag* next = endTag->next();
    if (next->isFree()) {
        vmState.merge(next->vmState());
        Range right(range.end(), next->size());
        range = Range(range.begin(), range.size() + right.size());

        endTag->clear();
        next->clear();

        endTag = LargeChunk::endTag(range.begin(), range.size());
    }

    beginTag->setRange(range);
    beginTag->setFree(true);
    beginTag->setVMState(vmState);
    endTag->init(beginTag);

    return LargeObject(beginTag, endTag, range.begin());
}

inline std::pair<LargeObject, LargeObject> LargeObject::split(size_t size) const
{
    Range split(begin(), size);
    Range leftover = Range(split.end(), this->size() - size);
    BASSERT(leftover.size() >= largeMin);

    BeginTag* splitBeginTag = m_beginTag;
    EndTag* splitEndTag = LargeChunk::endTag(split.begin(), size);

    BeginTag* leftoverBeginTag = LargeChunk::beginTag(leftover.begin());
    EndTag* leftoverEndTag = m_endTag;

    splitBeginTag->setRange(split);
    splitEndTag->init(splitBeginTag);

    *leftoverBeginTag = *splitBeginTag;
    leftoverBeginTag->setRange(leftover);
    leftoverEndTag->init(leftoverBeginTag);

    return std::make_pair(
        LargeObject(splitBeginTag, splitEndTag, split.begin()),
        LargeObject(leftoverBeginTag, leftoverEndTag, leftover.begin()));
}

inline void LargeObject::validateSelf() const
{
    BASSERT(!m_beginTag->isEnd());
    BASSERT(m_endTag->isEnd() || static_cast<BoundaryTag*>(m_endTag) == static_cast<BoundaryTag*>(m_beginTag));

    BASSERT(size() >= largeMin);

    BASSERT(m_beginTag->size() == m_endTag->size());
    BASSERT(m_beginTag->isFree() == m_endTag->isFree());
    BASSERT(m_beginTag->vmState() == m_endTag->vmState());
    BASSERT(m_beginTag->isMarked() == m_endTag->isMarked());
}

inline void LargeObject::validate() const
{
    if (!m_beginTag->prev()->isSentinel()) {
        LargeObject prev(DoNotValidate, begin() - m_beginTag->prev()->size());
        prev.validateSelf();
    }

    validateSelf();

    if (!m_endTag->next()->isSentinel()) {
        LargeObject next(DoNotValidate, begin() + size());
        next.validateSelf();
    }
}

inline Range LargeObject::init(LargeChunk* chunk)
{
    Range range(chunk->begin(), chunk->end() - chunk->begin());

    BeginTag* beginTag = LargeChunk::beginTag(range.begin());
    beginTag->setRange(range);
    beginTag->setFree(true);
    beginTag->setVMState(VMState::Virtual);

    EndTag* endTag = LargeChunk::endTag(range.begin(), range.size());
    endTag->init(beginTag);

    // Mark the left and right edges of our chunk as allocated. This naturally
    // prevents merging logic from overflowing beyond our chunk, without requiring
    // special-case checks.
    
    EndTag* leftSentinel = beginTag->prev();
    BASSERT(leftSentinel >= static_cast<void*>(chunk));
    leftSentinel->initSentinel();

    BeginTag* rightSentinel = endTag->next();
    BASSERT(rightSentinel < static_cast<void*>(range.begin()));
    rightSentinel->initSentinel();
    
    return range;
}

} // namespace bmalloc

#endif // LargeObject_h
