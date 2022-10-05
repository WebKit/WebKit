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

class ReturnChecker extends Visitor {
    constructor(program)
    {
        super();
        this.returnStyle = {
            DefinitelyReturns: "Definitely Returns",
            DefinitelyDoesntReturn: "Definitely Doesn't Return",
            HasntReturnedYet: "Hasn't Returned Yet"
        };
        this._program = program;
    }
    
    _mergeReturnStyle(a, b)
    {
        if (!a)
            return b;
        if (!b)
            return a;
        switch (a) {
        case this.returnStyle.DefinitelyReturns:
        case this.returnStyle.DefinitelyDoesntReturn:
            if (a == b)
                return a;
            return this.returnStyle.HasntReturnedYet;
        case this.returnStyle.HasntReturnedYet:
            return this.returnStyle.HasntReturnedYet;
        default:
            throw new Error("Bad return style: " + a);
        }
    }

    visitFuncDef(node)
    {
        if (node.returnType.equals(node.program.intrinsics.void))
            return;
        
        let bodyValue = node.body.visit(this);
        if (bodyValue == this.returnStyle.DefinitelyDoesntReturn || bodyValue == this.returnStyle.HasntReturnedYet)
            throw new WTypeError(node.origin.originString, "Function does not return");
    }
    
    visitBlock(node)
    {
        for (let statement of node.statements) {
            switch (statement.visit(this)) {
            case this.returnStyle.DefinitelyReturns:
                return this.returnStyle.DefinitelyReturns;
            case this.returnStyle.DefinitelyDoesntReturn:
                return this.returnStyle.DefinitelyDoesntReturn;
            case this.returnStyle.HasntReturnedYet:
                continue;
            }
        }
        return this.returnStyle.HasntReturnedYet;
    }

    visitIfStatement(node)
    {
        if (node.elseBody) {
            let bodyValue = node.body.visit(this);
            let elseValue = node.elseBody.visit(this);
            return this._mergeReturnStyle(bodyValue, elseValue);
        }
        return this.returnStyle.HasntReturnedYet;
    }

    _isBoolCastFromLiteralTrue(node)
    {
        return node.isCast && node.returnType instanceof TypeRef && node.returnType.equals(this._program.intrinsics.bool) && node.argumentList.length == 1 && node.argumentList[0] instanceof BoolLiteral && node.argumentList[0].value;
    }

    visitWhileLoop(node)
    {
        let bodyReturn = node.body.visit(this);
        if (node.conditional instanceof CallExpression && this._isBoolCastFromLiteralTrue(node.conditional)) {
            switch (bodyReturn) {
            case this.returnStyle.DefinitelyReturns:
                return this.returnStyle.DefinitelyReturns;
            case this.returnStyle.DefinitelyDoesntReturn:
            case this.returnStyle.HasntReturnedYet:
                return this.returnStyle.HasntReturnedYet;
            }
        }
        return this.returnStyle.HasntReturnedYet;
    }

    visitDoWhileLoop(node)
    {
        let result = this.returnStyle.HasntReturnedYet;
        switch (node.body.visit(this)) {
        case this.returnStyle.DefinitelyReturns:
            result = this.returnStyle.DefinitelyReturns;
        case this.returnStyle.DefinitelyDoesntReturn:
        case this.returnStyle.HasntReturnedYet:
            result = this.returnStyle.HasntReturnedYet;
        }
        return result;
    }

    visitForLoop(node)
    {
        let bodyReturn = node.body.visit(this);
        if (node.condition === undefined || this._isBoolCastFromLiteralTrue(node.condition)) {
            switch (bodyReturn) {
            case this.returnStyle.DefinitelyReturns:
                return this.returnStyle.DefinitelyReturns;
            case this.returnStyle.DefinitelyDoesntReturn:
            case this.returnStyle.HasntReturnedYet:
                return this.returnStyle.HasntReturnedYet;
            }
        }
    }
    
    visitSwitchStatement(node)
    {
        // FIXME: This seems like it's missing things. For example, we need to be smart about this:
        //
        // for (;;) {
        //     switch (...) {
        //     ...
        //         continue; // This continues the for loop
        //     }
        // }
        //
        // I'm not sure what that means for this analysis. I'm starting to think that the right way to
        // build this analysis is to run a visitor that builds a CFG and then analyze the CFG.
        // https://bugs.webkit.org/show_bug.cgi?id=177172
        
        let returnStyle = null;
        for (let switchCase of node.switchCases) {
            let bodyStyle = switchCase.body.visit(this);
            // FIXME: The fact that we need this demonstrates the need for CFG analysis.
            if (bodyStyle == this.returnStyle.DefinitelyDoesntReturn)
                bodyStyle = this.returnStyle.HasntReturnedYet;
            returnStyle = this._mergeReturnStyle(returnStyle, bodyStyle);
        }
        return returnStyle;
    }
    
    visitReturn(node)
    {
        return this.returnStyle.DefinitelyReturns;
    }
    
    visitTrapStatement(node)
    {
        return this.returnStyle.DefinitelyReturns;
    }

    visitBreak(node)
    {
        return this.returnStyle.DefinitelyDoesntReturn;
    }

    visitContinue(node)
    {
        // FIXME: This seems wrong. Consider a loop like:
        //
        // int foo() { for (;;) { continue; } }
        //
        // This program shouldn't claim that the problem is that it doesn't return.
        // https://bugs.webkit.org/show_bug.cgi?id=177172
        return this.returnStyle.DefinitelyDoesntReturn;
    }
}
