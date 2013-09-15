/*
 * Copyright (C) 2010 Alex Milowski (alex@milowski.com). All rights reserved.
 * Copyright (C) 2013 The MathJax Consortium.
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

#ifndef RenderMathMLScripts_h
#define RenderMathMLScripts_h

#if ENABLE(MATHML)

#include "RenderMathMLBlock.h"

namespace WebCore {
    
class RenderMathMLScripts;

class RenderMathMLScriptsWrapper : public RenderMathMLBlock {

friend class RenderMathMLScripts;

public:
    enum WrapperType { Base, SubSupPair };

    RenderMathMLScriptsWrapper(Element* element, WrapperType kind) :
    RenderMathMLBlock(element), m_kind(kind) { };

    virtual void addChild(RenderObject* child, RenderObject* beforeChild = 0) OVERRIDE;
    virtual void removeChild(RenderObject*) OVERRIDE;

private:
    static RenderMathMLScriptsWrapper* createAnonymousWrapper(RenderMathMLScripts* renderObject, WrapperType);

    void addChildInternal(bool normalInsertion, RenderObject* child, RenderObject* beforeChild = 0);
    void removeChildInternal(bool normalRemoval, RenderObject* child);

    virtual const char* renderName() const { return m_kind == Base ? "Base Wrapper" : "SubSupPair Wrapper"; }

    virtual bool isRenderMathMLScriptsWrapper() const OVERRIDE FINAL { return true; }

    RenderMathMLScripts* parentMathMLScripts();

    WrapperType m_kind;
};

inline RenderMathMLScriptsWrapper* toRenderMathMLScriptsWrapper(RenderMathMLBlock* block)
{
    ASSERT_WITH_SECURITY_IMPLICATION(!block || block->isRenderMathMLScriptsWrapper());
    return static_cast<RenderMathMLScriptsWrapper*>(block);
}

inline const RenderMathMLScriptsWrapper* toRenderMathMLScriptsWrapper(const RenderMathMLBlock* block)
{
    ASSERT_WITH_SECURITY_IMPLICATION(!block || block->isRenderMathMLScriptsWrapper());
    return static_cast<const RenderMathMLScriptsWrapper*>(block);
}

inline RenderMathMLScriptsWrapper* toRenderMathMLScriptsWrapper(RenderObject* object)
{
    return toRenderMathMLScriptsWrapper(toRenderMathMLBlock(object));
}

inline const RenderMathMLScriptsWrapper* toRenderMathMLScriptsWrapper(const RenderObject* object)
{
    return toRenderMathMLScriptsWrapper(toRenderMathMLBlock(object));
}

// This will catch anyone doing an unnecessary cast.
void toRenderMathMLScriptsWrapper(const RenderMathMLScriptsWrapper*);

// Render a base with scripts.
class RenderMathMLScripts : public RenderMathMLBlock {

friend class RenderMathMLScriptsWrapper;

public:
    RenderMathMLScripts(Element*);
    virtual void addChild(RenderObject* child, RenderObject* beforeChild = 0) OVERRIDE;
    virtual void removeChild(RenderObject*) OVERRIDE;
    
    virtual RenderMathMLOperator* unembellishedOperator();
    virtual int firstLineBoxBaseline() const OVERRIDE;

protected:
    virtual void layout();
    
private:
    void addChildInternal(bool normalInsertion, RenderObject* child, RenderObject* beforeChild = 0);
    void removeChildInternal(bool normalRemoval, RenderObject* child);

    virtual bool isRenderMathMLScripts() const OVERRIDE { return true; }
    void fixAnonymousStyleForSubSupPair(RenderObject* subSupPair, bool isPostScript);
    void fixAnonymousStyles();

    virtual void styleDidChange(StyleDifference, const RenderStyle* oldStyle) OVERRIDE;

    virtual const char* renderName() const { return "RenderMathMLScripts"; }

    // Omit our subscript and/or superscript. This may return 0 for a non-MathML base (which
    // won't occur in valid MathML).
    RenderBoxModelObject* base() const;

    enum ScriptsType { Sub, Super, SubSup, Multiscripts };

    ScriptsType m_kind;
    RenderMathMLScriptsWrapper* m_baseWrapper;
};

inline RenderMathMLScripts* toRenderMathMLScripts(RenderMathMLBlock* block)
{
    ASSERT_WITH_SECURITY_IMPLICATION(!block || block->isRenderMathMLScripts());
    return static_cast<RenderMathMLScripts*>(block);
}

inline const RenderMathMLScripts* toRenderMathMLScripts(const RenderMathMLBlock* block)
{
    ASSERT_WITH_SECURITY_IMPLICATION(!block || block->isRenderMathMLScripts());
    return static_cast<const RenderMathMLScripts*>(block);
}

inline RenderMathMLScripts* toRenderMathMLScripts(RenderObject* object)
{
    return toRenderMathMLScripts(toRenderMathMLBlock(object));
}

inline const RenderMathMLScripts* toRenderMathMLScripts(const RenderObject* object)
{
    return toRenderMathMLScripts(toRenderMathMLBlock(object));
}

// This will catch anyone doing an unnecessary cast.
void toRenderMathMLScripts(const RenderMathMLScripts*);

}

#endif // ENABLE(MATHML)

#endif // RenderMathMLScripts_h
