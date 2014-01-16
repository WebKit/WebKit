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

    virtual void addChild(RenderObject* child, RenderObject* beforeChild = 0) override;
    virtual void removeChild(RenderObject&) override;

private:
    RenderMathMLScriptsWrapper(Document& document, PassRef<RenderStyle> style, WrapperType kind)
        : RenderMathMLBlock(document, std::move(style))
        , m_kind(kind)
    {
    }

    static RenderMathMLScriptsWrapper* createAnonymousWrapper(RenderMathMLScripts* renderObject, WrapperType);

    void addChildInternal(bool normalInsertion, RenderObject* child, RenderObject* beforeChild = 0);
    void removeChildInternal(bool normalRemoval, RenderObject& child);

    virtual const char* renderName() const override { return m_kind == Base ? "Base Wrapper" : "SubSupPair Wrapper"; }
    virtual bool isRenderMathMLScriptsWrapper() const override final { return true; }

    RenderMathMLScripts* parentMathMLScripts();

    WrapperType m_kind;
};

RENDER_OBJECT_TYPE_CASTS(RenderMathMLScriptsWrapper, isRenderMathMLScriptsWrapper());

// Render a base with scripts.
class RenderMathMLScripts : public RenderMathMLBlock {

friend class RenderMathMLScriptsWrapper;

public:
    RenderMathMLScripts(Element&, PassRef<RenderStyle>);
    virtual void addChild(RenderObject* child, RenderObject* beforeChild = 0) override;
    virtual void removeChild(RenderObject&) override;
    
    virtual RenderMathMLOperator* unembellishedOperator();
    virtual int firstLineBaseline() const override;

protected:
    virtual void layout();
    
private:
    void addChildInternal(bool normalInsertion, RenderObject* child, RenderObject* beforeChild = 0);
    void removeChildInternal(bool normalRemoval, RenderObject& child);

    virtual bool isRenderMathMLScripts() const override final { return true; }
    virtual const char* renderName() const override { return "RenderMathMLScripts"; }

    void fixAnonymousStyleForSubSupPair(RenderObject* subSupPair, bool isPostScript);
    void fixAnonymousStyles();

    virtual void styleDidChange(StyleDifference, const RenderStyle* oldStyle) override;

    // Omit our subscript and/or superscript. This may return 0 for a non-MathML base (which
    // won't occur in valid MathML).
    RenderBoxModelObject* base() const;

    enum ScriptsType { Sub, Super, SubSup, Multiscripts };

    ScriptsType m_kind;
    RenderMathMLScriptsWrapper* m_baseWrapper;
};

RENDER_OBJECT_TYPE_CASTS(RenderMathMLScripts, isRenderMathMLScripts());

}

#endif // ENABLE(MATHML)

#endif // RenderMathMLScripts_h
