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
"use strict";

class BuiltInSemantic extends Semantic {
    constructor(origin, name, ...extraArguments)
    {
        super(origin);
        this._name = name;
        this._extraArguments = extraArguments;
    }

    get name() { return this._name; }
    get extraArguments() { return this._extraArguments; }

    isAcceptableType(type, program)
    {
        let requiredType;
        switch (this.name) {
        case "SV_InstanceID":
            requiredType = program.intrinsics.uint;
            break;
        case "SV_VertexID":
            requiredType = program.intrinsics.uint;
            break;
        case "PSIZE":
            requiredType = program.intrinsics.float;
            break;
        case "SV_Position":
            requiredType = program.intrinsics["vector<float, 4>"];
            break;
        case "SV_IsFrontFace":
            requiredType = program.intrinsics.bool;
            break;
        case "SV_SampleIndex":
            requiredType = program.intrinsics.uint;
            break;
        case "SV_InnerCoverage":
            requiredType = program.intrinsics.uint;
            break;
        case "SV_Target":
            requiredType = program.intrinsics["vector<float, 4>"];
            break;
        case "SV_Depth":
            requiredType = program.intrinsics.float;
            break;
        case "SV_Coverage":
            requiredType = program.intrinsics.uint;
            break;
        case "SV_DispatchThreadID":
            requiredType = program.intrinsics["vector<uint, 3>"];
            break;
        case "SV_GroupID":
            requiredType = program.intrinsics["vector<uint, 3>"];
            break;
        case "SV_GroupIndex":
            requiredType = program.intrinsics.uint;
            break;
        case "SV_GroupThreadID":
            requiredType = program.intrinsics["vector<uint, 3>"];
            break;
        default:
            throw new Error(`Unknown semantic: ${this.name}`);
        }

        return type.equals(requiredType);
    }

    isAcceptableForShaderType(direction, shaderType)
    {
        switch (shaderType) {
        case "vertex":
            switch (direction) {
            case "input":
                switch (this.name) {
                case "SV_InstanceID":
                case "SV_VertexID":
                    return true;
                default:
                    return false;
                }
            case "output":
                switch (this.name) {
                case "PSIZE":
                case "SV_Position":
                    return true;
                default:
                    return false;
                }
            }
        case "fragment":
            switch (direction) {
            case "input":
                switch (this.name) {
                case "SV_IsFrontFace":
                case "SV_SampleIndex":
                case "SV_InnerCoverage":
                    return true;
                default:
                    return false;
                }
            case "output":
                switch (this.name) {
                case "SV_Target":
                case "SV_Depth":
                case "SV_Coverage":
                    return true;
                default:
                    return false;
                }
            }
        case "compute":
            if (direction != "input")
                return false;
            switch (this.name) {
            case "SV_DispatchThreadID":
            case "SV_GroupID":
            case "SV_GroupIndex":
            case "SV_GroupThreadID":
                return true;
            default:
                return false;
            }
        case "test":
            return true;
        default:
            throw new Error(`Unknown shade type: ${shaderType}`);
        }
    }

    toString()
    {
        let result = this.name;
        if (this.extraArguments)
            result += this.extraArguments.join("");
        return result;
    }
}

