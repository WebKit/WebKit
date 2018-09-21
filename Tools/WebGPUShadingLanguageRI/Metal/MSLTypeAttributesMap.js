/*
 * Copyright (C) 2018 Apple Inc. All rights reserved.
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

// Provides lookup for all the top level types in the program.
class MSLTypeAttributesMap {

    constructor(functionDefs, typeUnifier)
    {
        this._typeUnifier = typeUnifier;
        this._typeAttributeMap = new Map();

        for (let funcDef of functionDefs) {
            if (funcDef.shaderType == "vertex")
                this._visitVertexShader(funcDef);
            else if (funcDef.shaderType == "fragment")
                this._visitFragmentShader(funcDef);
        }
    }

    attributesForType(type)
    {
        const key = this._typeUnifier.uniqueTypeId(type);
        let attrs = this._typeAttributeMap.get(key);
        if (!attrs)
            this._typeAttributeMap.set(key, attrs = new MSLTypeAttributes(type));
        return attrs;
    }

    _visitVertexShader(func)
    {
        this.attributesForType(func.returnType).isVertexOutputOrFragmentInput = true;
        for (let param of func.parameters)
            this.attributesForType(param.type).isVertexAttribute = true;
    }

    _visitFragmentShader(func)
    {
        this.attributesForType(func.returnType).isFragmentOutput = true;
        for (let param of func.parameters)
            this.attributesForType(param.type).isVertexOutputOrFragmentInput = true;
    }

    // FIXME: Support compute shaders.
}