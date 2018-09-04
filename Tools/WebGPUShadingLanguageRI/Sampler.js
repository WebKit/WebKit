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

class Sampler {
    constructor(options)
    {
        if (options.rAddressMode == undefined)
            this._rAddressMode = "clampToEdge";
        else
            this._rAddressMode = options.rAddressMode;
        if (options.sAddressMode == undefined)
            this._sAddressMode = "clampToEdge";
        else
            this._sAddressMode = options.sAddressMode;
        if (options.tAddressMode == undefined)
            this._tAddressMode = "clampToEdge";
        else
            this._tAddressMode = options.tAddressMode;
        if (options.minFilter == undefined)
            this._minFilter = "nearest";
        else
            this._minFilter = options.minFilter;
        if (options.magFilter == undefined)
            this._magFilter = "nearest";
        else
            this._magFilter = options.magFilter;
        if (options.mipmapFilter == undefined)
            this._mipmapFilter = "nearest";
        else
            this._mipmapFilter = options.mipmapFilter;
        if (options.lodMinClamp == undefined)
            this._lodMinClamp = -Number.MAX_VALUE;
        else
            this._lodMinClamp = options.lodMinClamp;
        if (options.lodMaxClamp == undefined)
            this._lodMaxClamp = Number.MAX_VALUE;
        else
            this._lodMaxClamp = options.lodMaxClamp;
        if (options.maxAnisotropy == undefined)
            this._maxAnisotropy = 1;
        else
            this._maxAnisotropy = options.maxAnisotropy;
        if (options.compareFunction == undefined)
            this._compareFunction = "never";
        else
            this._compareFunction = options.compareFunction;
        if (options.borderColor == undefined)
            this._borderColor = "transparentBlack";
        else
            this._borderColor = options.borderColor;
    }
    
    get rAddressMode() { return this._rAddressMode; }
    get sAddressMode() { return this._sAddressMode; }
    get tAddressMode() { return this._tAddressMode; }
    get minFilter() { return this._minFilter; }
    get magFilter() { return this._magFilter; }
    get mipmapFilter() { return this._mipmapFilter; }
    get lodMinClamp() { return this._lodMinClamp; }
    get lodMaxClamp() { return this._lodMaxClamp; }
    get maxAnisotropy() { return this._maxAnisotropy; }
    get compareFunction() { return this._compareFunction; }
    get borderColor() { return this._borderColor; }

    calculateBorderColor(innerType)
    {
        // Vulkan 1.1.83 section 15.3.3
        function computeValues() {
            switch (innerType) {
                case "uchar":
                    return {transparentBlack: 0, opaqueBlack: 0, opaqueWhite: 0xFF};
                case "ushort":
                    return {transparentBlack: 0, opaqueBlack: 0, opaqueWhite: 0xFFFF};
                case "uint":
                    return {transparentBlack: 0, opaqueBlack: 0, opaqueWhite: 0xFFFFFFFF};
                case "char":
                    return {transparentBlack: 0, opaqueBlack: 0, opaqueWhite: 0x7F};
                case "short":
                    return {transparentBlack: 0, opaqueBlack: 0, opaqueWhite: 0x7FFF};
                case "int":
                    return {transparentBlack: 0, opaqueBlack: 0, opaqueWhite: 0x7FFFFFFF};
                case "half":
                    return {transparentBlack: 0, opaqueBlack: 0, opaqueWhite: 1};
                case "float":
                    return {transparentBlack: 0, opaqueBlack: 0, opaqueWhite: 1};
                case "uchar2":
                    return {transparentBlack: [0, 0], opaqueBlack: [0, 0], opaqueWhite: [0xFF, 0xFF]};
                case "ushort2":
                    return {transparentBlack: [0, 0], opaqueBlack: [0, 0], opaqueWhite: [0xFFFF, 0xFFFF]};
                case "uint2":
                    return {transparentBlack: [0, 0], opaqueBlack: [0, 0], opaqueWhite: [0xFFFFFFFF, 0xFFFFFFFF]};
                case "char2":
                    return {transparentBlack: [0, 0], opaqueBlack: [0, 0], opaqueWhite: [0x7F, 0x7F]};
                case "short2":
                    return {transparentBlack: [0, 0], opaqueBlack: [0, 0], opaqueWhite: [0x7FFF, 0x7FFF]};
                case "int2":
                    return {transparentBlack: [0, 0], opaqueBlack: [0, 0], opaqueWhite: [0x7FFFFFFF, 0x7FFFFFFF]};
                case "half2":
                    return {transparentBlack: [0, 0], opaqueBlack: [0, 0], opaqueWhite: [1, 1]};
                case "float2":
                    return {transparentBlack: [0, 0], opaqueBlack: [0, 0], opaqueWhite: [1, 1]};
                case "uchar3":
                    return {transparentBlack: [0, 0, 0], opaqueBlack: [0, 0, 0], opaqueWhite: [0xFF, 0xFF, 0xFF]};
                case "ushort3":
                    return {transparentBlack: [0, 0, 0], opaqueBlack: [0, 0, 0], opaqueWhite: [0xFFFF, 0xFFFF, 0xFFFF]};
                case "uint3":
                    return {transparentBlack: [0, 0, 0], opaqueBlack: [0, 0, 0], opaqueWhite: [0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF]};
                case "char3":
                    return {transparentBlack: [0, 0, 0], opaqueBlack: [0, 0, 0], opaqueWhite: [0x7F, 0x7F, 0x7F]};
                case "short3":
                    return {transparentBlack: [0, 0, 0], opaqueBlack: [0, 0, 0], opaqueWhite: [0x7FFF, 0x7FFF, 0x7FFF]};
                case "int3":
                    return {transparentBlack: [0, 0, 0], opaqueBlack: [0, 0, 0], opaqueWhite: [0x7FFFFFFF, 0x7FFFFFFF, 0x7FFFFFFF]};
                case "half3":
                    return {transparentBlack: [0, 0, 0], opaqueBlack: [0, 0, 0], opaqueWhite: [1, 1, 1]};
                case "float3":
                    return {transparentBlack: [0, 0, 0], opaqueBlack: [0, 0, 0], opaqueWhite: [1, 1, 1]};
                case "uchar4":
                    return {transparentBlack: [0, 0, 0, 0], opaqueBlack: [0, 0, 0, 0xFF], opaqueWhite: [0xFF, 0xFF, 0xFF, 0xFF]};
                case "ushort4":
                    return {transparentBlack: [0, 0, 0, 0], opaqueBlack: [0, 0, 0, 0xFFFF], opaqueWhite: [0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF]};
                case "uint4":
                    return {transparentBlack: [0, 0, 0, 0], opaqueBlack: [0, 0, 0, 0xFFFFFFFF], opaqueWhite: [0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF]};
                case "char4":
                    return {transparentBlack: [0, 0, 0, 0], opaqueBlack: [0, 0, 0, 0x7F], opaqueWhite: [0x7F, 0x7F, 0x7F, 0x7F]};
                case "short4":
                    return {transparentBlack: [0, 0, 0, 0], opaqueBlack: [0, 0, 0, 0x7FFF], opaqueWhite: [0x7FFF, 0x7FFF, 0x7FFF, 0x7FFF]};
                case "int4":
                    return {transparentBlack: [0, 0, 0, 0], opaqueBlack: [0, 0, 0, 0x7FFFFFFF], opaqueWhite: [0x7FFFFFFF, 0x7FFFFFFF, 0x7FFFFFFF, 0x7FFFFFFF]};
                case "half4":
                    return {transparentBlack: [0, 0, 0, 0], opaqueBlack: [0, 0, 0, 1], opaqueWhite: [1, 1, 1, 1]};
                case "float4":
                    return {transparentBlack: [0, 0, 0, 0], opaqueBlack: [0, 0, 0, 1], opaqueWhite: [1, 1, 1, 1]};
                default:
                    throw new Error("Unknown texture inner type");
            }
        }
        return computeValues()[this.borderColor];
    }
}
