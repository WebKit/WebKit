/*
 * Copyright (C) 2017 Apple Inc. All rights reserved.
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
"use strict";

class LoopChecker extends Visitor {
    constructor()
    {
        super();
        this._loopDepth = 0;
        this._switchDepth = 0;
    }

    visitFuncDef(node)
    {
        if (this._loopDepth != 0)
            throw new WTypeError(node.origin.originString, "LoopChecker does not understand nested functions.");
        super.visitFuncDef(node);
    }

    visitWhileLoop(node)
    {
        node.conditional.visit(this);
        ++this._loopDepth;
        node.body.visit(this);
        if (this._loopDepth == 0)
            throw new WTypeError(node.origin.originString, "The number of nested loops is negative!");
        --this._loopDepth;
    }

    visitDoWhileLoop(node)
    {
        ++this._loopDepth;
        node.body.visit(this);
        if (this._loopDepth == 0)
            throw new WTypeError(node.origin.originString, "The number of nested loops is negative!");
        --this._loopDepth;
        node.conditional.visit(this);
    }

    visitForLoop(node)
    {
        if (node.initialization)
            node.initialization.visit(this);
        if (node.condition)
            node.condition.visit(this);
        if (node.increment)
            node.increment.visit(this);
        ++this._loopDepth;
        node.body.visit(this);
        if (this._loopDepth == 0)
            throw new WTypeError(node.origin.originString, "The number of nested loops is negative!");
        --this._loopDepth;
    }
    
    visitSwitchStatement(node)
    {
        node.value.visit(this);
        this._switchDepth++;
        for (let switchCase of node.switchCases)
            switchCase.visit(this);
        this._switchDepth--;
    }
    
    visitBreak(node)
    {
        if (this._loopDepth == 0 && this._switchDepth == 0)
            throw new WTypeError(node.origin.originString, "Break statement without enclosing loop or switch!");
        super.visitBreak(node);
    }
    
    visitContinue(node)
    {
        if (this._loopDepth == 0)
            throw new WTypeError(node.origin.originString, "Continue statement without enclosing loop!");
        super.visitContinue(node);
    }
}
