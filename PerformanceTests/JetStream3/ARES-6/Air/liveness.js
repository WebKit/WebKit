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

class Liveness {
    constructor(thing, code)
    {
        this._thing = thing;
        this._code = code;
        
        this._liveAtHead = new Map();
        this._liveAtTail = new Map();
        
        for (let block of code) {
            this._liveAtHead.set(block, new Set());
            
            let liveAtTail = new Set();
            this._liveAtTail.set(block, liveAtTail);
            
            block.last.forEach(
                thing,
                (value, role, type, width) => {
                    if (Arg.isLateUse(role))
                        liveAtTail.add(value);
                });
        }
        
        let dirtyBlocks = new Set(code);
        
        let changed;
        do {
            changed = false;
            
            for (let blockIndex = code.size; blockIndex--;) {
                let block = code.at(blockIndex);
                if (!block)
                    continue;
                
                if (!dirtyBlocks.delete(block))
                    continue;
                
                let localCalc = this.localCalc(block);
                for (let instIndex = block.size; instIndex--;)
                    localCalc.execute(instIndex);
                
                // Handle the early def's of the first instruction.
                block.at(0).forEach(
                    thing,
                    (value, role, type, width) => {
                        if (Arg.isEarlyDef(role))
                            localCalc.liveSet.remove(value);
                    });
                
                let liveAtHead = this._liveAtHead.get(block);
                
                if (!mergeIntoSet(liveAtHead, localCalc.liveSet))
                    continue;
                
                for (let predecessor of block.predecessors) {
                    if (mergeIntoSet(this._liveAtTail.get(predecessor), liveAtHead)) {
                        dirtyBlocks.add(predecessor);
                        changed = true;
                    }
                }
            }
        } while (changed);
    }
    
    get thing() { return this._thing; }
    get code() { return this._code; }
    get liveAtHead() { return this._liveAtHead; }
    get liveAtTail() { return this._liveAtTail; }
    
    localCalc(block)
    {
        let liveness = this;
        class LocalCalc {
            constructor()
            {
                this._liveSet = new Set(liveness.liveAtTail.get(block));
            }
            
            get liveSet() { return this._liveSet; }
            
            execute(instIndex)
            {
                let inst = block.at(instIndex);
                
                // First handle the early defs of the next instruction.
                if (instIndex + 1 < block.size) {
                    block.at(instIndex + 1).forEach(
                        liveness.thing,
                        (value, role, type, width) => {
                            if (Arg.isEarlyDef(role))
                                this._liveSet.delete(value);
                        });
                }
                
                // Then handle defs.
                inst.forEach(
                    liveness.thing,
                    (value, role, type, width) => {
                        if (Arg.isLateDef(role))
                            this._liveSet.delete(value);
                    });
                
                // Then handle uses.
                inst.forEach(
                    liveness.thing,
                    (value, role, type, width) => {
                        if (Arg.isEarlyUse(role))
                            this._liveSet.add(value);
                    });
                
                // Finally handle the late uses of the previous instruction.
                if (instIndex - 1 >= 0) {
                    block.at(instIndex - 1).forEach(
                        liveness.thing,
                        (value, role, type, width) => {
                            if (Arg.isLateUse(role))
                                this._liveSet.add(value);
                        });
                }
            }
        }
        
        return new LocalCalc();
    }
}

