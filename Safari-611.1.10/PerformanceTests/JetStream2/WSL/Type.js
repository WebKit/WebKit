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

class Type extends Node {
    get typeParameters() { return []; }
    get kind() { return Type; }
    get isPtr() { return false; }
    get isArray() { return false; }
    get isArrayRef() { return false; }
    get isRef() { return this.isPtr || this.isArrayRef; }
    get isNumber() { return false; }
    get isInt() { return false; }
    get isFloating() { return false; }
    get isPrimitive() { return false; }
    
    inherits(protocol)
    {
        if (!protocol)
            return {result: true};
        return protocol.hasHeir(this);
    }
    
    get instantiatedType() { return this.visit(new InstantiateImmediates()); }
    
    // Have to call these on the unifyNode.
    argumentForAndOverload(origin, value)
    {
        return new MakePtrExpression(origin, value);
    }
    argumentTypeForAndOverload(origin)
    {
        return new PtrType(origin, "thread", this);
    }
    returnTypeFromAndOverload(origin)
    {
        throw new WTypeError(origin.originString, "By-pointer overload returned non-pointer type: " + this);
    }
}

