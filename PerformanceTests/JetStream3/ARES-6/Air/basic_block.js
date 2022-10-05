/*
 * Copyright (C) 2016 Apple Inc. All rights reserved.
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

class BasicBlock {
    constructor(index, frequency)
    {
        this._index = index;
        this._frequency = frequency;
        this._insts = [];
        this._successors = [];
        this._predecessors = [];
    }
    
    get index() { return this._index; }
    get size() { return this._insts.length; }
    
    [Symbol.iterator]()
    {
        return this._insts[Symbol.iterator]();
    }
    
    at(index)
    {
        if (index >= this._insts.length)
            throw new Error("Out of bounds access");
        return this._insts[index];
    }
    
    get(index)
    {
        if (index < 0 || index >= this._insts.length)
            return null;
        return this._insts[index];
    }
    
    get last()
    {
        return this._insts[this._insts.length - 1];
    }
    
    get insts() { return this._insts; }
    
    append(inst) { this._insts.push(inst); }
    
    get numSuccessors() { return this._successors.length; }
    successor(index) { return this._successors[index]; }
    get successors() { return this._successors; }
    
    successorBlock(index) { return this._successors[index].block; }
    get successorBlocks()
    {
        return new Proxy(this._successors, {
            get(target, property) {
                if (typeof property == "string"
                    && (property | 0) == property)
                    return target[property].block;
                return target[property];
            },
            
            set(target, property, value) {
                if (typeof property == "string"
                    && (property | 0) == property) {
                    var oldValue = target[property];
                    target[property] = new FrequentedBlock(
                        value, oldValue ? oldValue.frequency : Normal);
                    return;
                }
                
                target[property] = value;
            }
        });
    }
    
    get numPredecessors() { return this._predecessors.length; }
    predecessor(index) { return this._predecessors[index]; }
    get predecessors() { return this._predecessors; }
    
    get frequency() { return this._frequency; }

    toString()
    {
        return "#" + this._index;
    }
    
    get headerString()
    {
        let result = "";
        result += `BB${this}: ; frequency = ${this._frequency}\n`;
        if (this._predecessors.length)
            result += "  Predecessors: " + this._predecessors.join(", ") + "\n";
        return result;
    }
    
    get footerString()
    {
        let result = "";
        if (this._successors.length)
            result += "  Successors: " + this._successors.join(", ") + "\n";
        return result;
    }
    
    toStringDeep()
    {
        let result = "";
        result += this.headerString;
        for (let inst of this)
            result += `    ${inst}\n`;
        result += this.footerString;
        return result;
    }
}

