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

class Program extends Node {
    constructor()
    {
        super();
        this._topLevelStatements = [];
        this._functions = new Map();
        this._types = new Map();
        this._protocols = new Map();
        this._funcInstantiator = new FuncInstantiator(this);
        this._globalNameContext = new NameContext();
        this._globalNameContext.program = this;
        this._globalNameContext.recognizeIntrinsics();
        this.intrinsics = this._globalNameContext.intrinsics;
    }
    
    get topLevelStatements() { return this._topLevelStatements; }
    get functions() { return this._functions; }
    get types() { return this._types; }
    get protocols() { return this._protocols; }
    get funcInstantiator() { return this._funcInstantiator; }
    get globalNameContext() { return this._globalNameContext; }
    
    add(statement)
    {
        statement.program = this;
        if (statement instanceof Func) {
            let array = this._functions.get(statement.name);
            if (!array)
                this._functions.set(statement.name, array = []);
            array.push(statement);
        } else if (statement instanceof Type)
            this._types.set(statement.name, statement);
        else if (statement instanceof Protocol)
            this._protocols.set(statement.name, statement);
        else
            throw new Error("Statement is not a function or type: " + statement);
        this._topLevelStatements.push(statement);
        this._globalNameContext.add(statement);
    }
    
    toString()
    {
        if (!this._topLevelStatements.length)
            return "";
        return this._topLevelStatements.join(";") + ";";
    }
}

