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

class Insertion {
    constructor(index, element)
    {
        this._index = index;
        this._element = element;
    }
    
    get index() { return this._index; }
    get element() { return this._element; }
    
    lessThan(other)
    {
        return this._index < other._index;
    }
}

class InsertionSet {
    constructor()
    {
        this._insertions = []
    }
    
    appendInsertion(insertion)
    {
        this._insertions.push(insertion);
    }
    
    append(index, element)
    {
        this.appendInsertion(new Insertion(index, element));
    }
    
    execute(target)
    {
        // We bubble-sort because that's what the C++ code, and for the same reason as we do it:
        // the stdlib doesn't have a stable sort and mergesort is slower in the common case of the
        // array usually being sorted. This array is usually sorted.
        bubbleSort(this._insertions, (a, b) => (a.lessThan(b)));
        
        let numInsertions = this._insertions.length;
        if (!numInsertions)
            return 0;
        let originalTargetSize = target.length;
        target.length += numInsertions;
        let lastIndex = target.length;
        for (let indexInInsertions = numInsertions; indexInInsertions--;) {
            let insertion = this._insertions[indexInInsertions];
            if (indexInInsertions && insertion.index < this._insertions[indexInInsertions - 1].index)
                throw new Error("Insertions out of order");
            if (insertion.index > originalTargetSize)
                throw new Error("Out-of-bounds insertion");
            let firstIndex = insertion.index + indexInInsertions;
            let indexOffset = indexInInsertions + 1;
            for (let i = lastIndex; --i > firstIndex;)
                target[i] = target[i - indexOffset];
            target[firstIndex] = insertion.element;
            lastIndex = firstIndex;
        }
        this._insertions = [];
        return numInsertions;
    }
}

