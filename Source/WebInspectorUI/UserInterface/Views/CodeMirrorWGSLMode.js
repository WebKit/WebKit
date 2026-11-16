/*
 * Copyright (C) 2023 Devin Rousso <webkit@devinrousso.com>. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

CodeMirror.defineMode("wgsl", function(config) {
    function words(strings) {
        let result = {};
        for (let string of strings)
            result[string] = true;
        return result;
    }

    function tokenizeComment(stream, state) {
        while (!stream.eol()) {
            if (stream.match("/*"))
                ++state.commentDepth;
            else if (stream.match("*/")) {
                if (!--state.commentDepth) {
                    state.tokenize = null;
                    break;
                }
            } else
                stream.next();
        }
        return "comment";
    }

    return CodeMirror.getMode(config, {
        name: "clike",
        keywords: words([
            "alias",
            "break",
            "case",
            "const",
            "const_assert",
            "continue",
            "continuing",
            "default",
            "diagnostic",
            "discard",
            "else",
            "enable",
            "fn",
            "for",
            "if",
            "let",
            "loop",
            "override",
            "requires",
            "return",
            "struct",
            "switch",
            "var",
            "while",
        ]),
        types: words([
            "array",
            "atomic",
            "bool",
            "f16",
            "f32",
            "i32",
            "mat2x2",
            "mat2x2f",
            "mat2x2h",
            "mat2x3",
            "mat2x3f",
            "mat2x3h",
            "mat2x4",
            "mat2x4f",
            "mat2x4h",
            "mat3x2",
            "mat3x2f",
            "mat3x2h",
            "mat3x3",
            "mat3x3f",
            "mat3x3h",
            "mat3x4",
            "mat3x4f",
            "mat3x4h",
            "mat4x2",
            "mat4x2f",
            "mat4x2h",
            "mat4x3",
            "mat4x3f",
            "mat4x3h",
            "mat4x4",
            "mat4x4f",
            "mat4x4h",
            "ptr",
            "sampler",
            "sampler_comparison",
            "texture_1d",
            "texture_2d",
            "texture_2d_array",
            "texture_3d",
            "texture_cube",
            "texture_cube_array",
            "texture_depth_2d",
            "texture_depth_2d_array",
            "texture_depth_cube",
            "texture_depth_cube_array",
            "texture_depth_multisampled_2d",
            "texture_external",
            "texture_multisampled_2d",
            "texture_storage_1d",
            "texture_storage_2d",
            "texture_storage_2d_array",
            "texture_storage_3d",
            "u32",
            "vec2",
            "vec2f",
            "vec2h",
            "vec2i",
            "vec2u",
            "vec3",
            "vec3f",
            "vec3h",
            "vec3i",
            "vec3u",
            "vec4",
            "vec4f",
            "vec4h",
            "vec4i",
            "vec4u",
        ]),
        builtin: words([
            "abs",
            "acos",
            "acosh",
            "all",
            "any",
            "arrayLength",
            "asin",
            "asinh",
            "atan",
            "atan2",
            "atanh",
            "atomicAdd",
            "atomicAnd",
            "atomicCompareExchangeWeak",
            "atomicExchange",
            "atomicLoad",
            "atomicMax",
            "atomicMin",
            "atomicOr",
            "atomicStore",
            "atomicSub",
            "atomicXor",
            "bitcast",
            "ceil",
            "clamp",
            "cos",
            "cosh",
            "countLeadingZeros",
            "countOneBits",
            "countTrailingZeros",
            "cross",
            "degrees",
            "determinant",
            "distance",
            "dot",
            "dot4I8Packed",
            "dot4U8Packed",
            "dpdx",
            "dpdxCoarse",
            "dpdxFine",
            "dpdy",
            "dpdyCoarse",
            "dpdyFine",
            "exp",
            "exp2",
            "extractBits",
            "faceForward",
            "firstLeadingBit",
            "firstTrailingBit",
            "floor",
            "fma",
            "fract",
            "frexp",
            "fwidth",
            "fwidthCoarse",
            "fwidthFine",
            "insertBits",
            "inverseSqrt",
            "ldexp",
            "length",
            "log",
            "log2",
            "max",
            "min",
            "mix",
            "modf",
            "normalize",
            "pack2x16float",
            "pack2x16snorm",
            "pack2x16unorm",
            "pack4x8snorm",
            "pack4x8unorm",
            "pack4xI8",
            "pack4xI8Clamp",
            "pack4xU8",
            "pack4xU8Clamp",
            "pow",
            "quantizeToF16",
            "radians",
            "reflect",
            "refract",
            "reverseBits",
            "round",
            "saturate",
            "select",
            "sign",
            "sin",
            "sinh",
            "smoothstep",
            "sqrt",
            "step",
            "storageBarrier",
            "tan",
            "tanh",
            "textureBarrier",
            "textureDimensions",
            "textureGather",
            "textureGatherCompare",
            "textureLoad",
            "textureNumLayers",
            "textureNumLevels",
            "textureNumSamples",
            "textureSample",
            "textureSampleBaseClampToEdge",
            "textureSampleBias",
            "textureSampleCompare",
            "textureSampleCompareLevel",
            "textureSampleGrad",
            "textureSampleLevel",
            "textureStore",
            "transpose",
            "trunc",
            "unpack2x16float",
            "unpack2x16snorm",
            "unpack2x16unorm",
            "unpack4x8snorm",
            "unpack4x8unorm",
            "unpack4xI8",
            "unpack4xU8",
            "workgroupBarrier",
            "workgroupUniformLoad",
        ]),
        atoms: words([
            "true",
            "false",
        ]),
        blockKeywords: words([
            "case",
            "continuing",
            "default",
            "else",
            "fn",
            "for",
            "if",
            "loop",
            "struct",
            "switch",
            "while",
        ]),
        defKeywords: words([
            "alias",
            "fn",
            "struct",
        ]),
        number: /^(?:0x(?:[\da-f]+(?:\.[\da-f]*)?|\.[\da-f]+)(?:p[+-]?\d+)?|(?:\d+\.?\d*|\.\d+)(?:e[+-]?\d+)?)[iufh]?/i,
        isOperatorChar: /[+\-*&%=<>!|/^~]/,
        hooks: {
            "@": function(stream) {
                stream.eatWhile(/\w/);
                return "meta";
            },
            "/": function(stream, state) {
                if (!stream.eat("*"))
                    return false;

                state.commentDepth = 1;
                state.tokenize = tokenizeComment;
                return tokenizeComment(stream, state);
            },
        },
    });
}, "clike");

CodeMirror.defineMIME("text/wgsl", "wgsl");
