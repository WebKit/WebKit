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

class NameFinder extends Visitor {
    constructor()
    {
        super();
        this._set = new Set();
        this._worklist = [];
    }
    
    get set() { return this._set; }
    get worklist() { return this._worklist; }
    
    add(name)
    {
        if (this._set.has(name))
            return;
        this._set.add(name);
        this._worklist.push(name);
    }
    
    visitProtocolRef(node)
    {
        this.add(node.name);
        super.visitProtocolRef(node);
    }
    
    visitTypeRef(node)
    {
        this.add(node.name);
        super.visitTypeRef(node);
    }
    
    visitVariableRef(node)
    {
        this.add(node.name);
        super.visitVariableRef(node);
    }
    
    visitTypeOrVariableRef(node)
    {
        this.add(node.name);
    }

    _handlePropertyAccess(node)
    {
        this.add(node.getFuncName);
        this.add(node.setFuncName);
        this.add(node.andFuncName);
    }
    
    visitDotExpression(node)
    {
        this._handlePropertyAccess(node);
        super.visitDotExpression(node);
    }
    
    visitIndexExpression(node)
    {
        this._handlePropertyAccess(node);
        super.visitIndexExpression(node);
    }
    
    visitCallExpression(node)
    {
        this.add(node.name);
        super.visitCallExpression(node);
    }
}

