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

class EPtr extends EValue {
    constructor(type, buffer, offset)
    {
        super(type);
        if (!(type.unifyNode instanceof PtrType))
            throw new Error("Cannot create EPtr with non-ptr type: " + type);
        this._buffer = buffer;
        this._offset = offset;
    }
    
    // The interpreter always passes around pointers to things. This means that sometimes we will
    // want to return a value but we have to "invent" a pointer to that value. No problem, this
    // function is here to help.
    //
    // In a real execution environment, uses of this manifest as SSA temporaries.
    static box(value)
    {
        if (!value.type)
            throw new Error("Value has no type: " + value);
        let buffer = new EBuffer(1);
        buffer.set(0, value);
        return new EPtr(new PtrType(null, "thread", value.type), buffer, 0);
    }
    
    get buffer() { return this._buffer; }
    get offset() { return this._offset; }
    
    loadValue()
    {
        return this.buffer.get(this.offset);
    }
    
    copyFrom(other)
    {
        let elementType = this.type.elementType;
        if (!elementType)
            throw new Error("No element type: " + this);
        for (let i = elementType.size; i--;)
            this.buffer.set(i, other.buffer.get(i));
    }
    
    toString()
    {
        if (!this.buffer)
            return "null";
        return this.type + "B" + this.buffer.index + ":" + this.offset;
    }
}

