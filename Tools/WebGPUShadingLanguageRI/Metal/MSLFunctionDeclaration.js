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

// Emits code for the first line of a function declaration or definition.
class MSLFunctionDeclaration {

    constructor(funcMangler, funcDef, typeUnifier, typeAttributes)
    {
        this._funcMangler = funcMangler;
        this._func = funcDef;
        this._typeUnifier = typeUnifier;
        this._typeAttributes = typeAttributes;
    }

    get funcMangler()
    {
        return this._funcMangler;
    }

    get func()
    {
        return this._func;
    }

    get typeUnifier()
    {
        return this._typeUnifier;
    }

    get typeAttributes()
    {
        return this._typeAttributes;
    }

    get isVertexShader()
    {
        return this._func.shaderType == "vertex";
    }

    get paramMap()
    {
        const map = new Map();
        let counter = 0;
        for (let param of this._func.parameters)
            map.set(param, `P${counter++}`);
        return map;
    }

    commentLine()
    {
        const params = [];
        for (let param of this.paramMap.keys())
            params.push(param.name);
        return `// ${this._func.name}(${params.join(", ")}) @ ${this._func.origin.originString}\n`;
    }

    toString()
    {
        let declLine = "";

        if (this.isShader)
            declLine += `${this._func.shaderType} `;
        declLine += `${this._typeUnifier.uniqueTypeId(this._func.returnType)} `;
        declLine += this._funcMangler.mangle(this.func);
        declLine += "("

        let params = [];
        const paramMap = this.paramMap;
        for (let param of this._func.parameters) {
            let pStr = `${this._typeUnifier.uniqueTypeId(param.type)} ${paramMap.get(param)}`;
            // FIXME: The parser doesn't currently support vertex shaders having uint parameters, so this doesn't work.
            if (this.parameterIsVertexId(param) && this.isVertexShader)
                pStr += " [[vertex_id]]";
            else if (this.parameterIsAttribute(param))
                pStr += " [[stage_in]]";
            params.push(pStr);
        }

        declLine += params.join(", ") + ")";

        return declLine;
    }

    get isShader()
    {
        // FIXME: Support WHLSL "compute" shaders (MSL calls these kernel shaders)
        return this.func.shaderType === "vertex" || this.func.shaderType === "fragment";
    }

    parameterIsAttribute(node)
    {
        // We currently assuming that all parameters to entry points are attributes.
        // TODO: Better logic for this, i.e. support samplers.
        return this.isShader;
    }

    parameterIsVertexId(node)
    {
        // FIXME: This isn't final, and isn't formally specified yet.
        return this.isVertexShader && node.name == "wsl_vertexID" && node.type.name == "uint32";
    }
}