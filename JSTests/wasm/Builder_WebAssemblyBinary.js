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

import * as assert from 'assert.js';
import LowLevelBinary from 'LowLevelBinary.js';
import * as WASM from 'WASM.js';

const put = (bin, type, value) => bin[type](value);

const emitters = {
    Type: (section, bin) => {
        put(bin, "varuint32", section.data.length);
        for (const entry of section.data) {
            const funcTypeConstructor = -0x20; // FIXME Move this to wasm.json.
            put(bin, "varint7", funcTypeConstructor);
            put(bin, "varuint32", entry.params.length);
            for (const param of entry.params)
                put(bin, "uint8", WASM.valueTypeValue[param]);
            if (entry.ret === "void")
                put(bin, "varuint1", 0);
            else {
                put(bin, "varuint1", 1);
                put(bin, "uint8", WASM.valueTypeValue[entry.ret]);
            }
        }
    },
    Import: (section, bin) => {
        put(bin, "varuint32", section.data.length);
        for (const entry of section.data) {
            put(bin, "string", entry.module);
            put(bin, "string", entry.field);
            put(bin, "uint8", WASM.externalKindValue[entry.kind]);
            switch (entry.kind) {
            default: throw new Error(`Implementation problem: unexpected kind ${entry.kind}`);
            case "Function": put(bin, "varuint32", entry.type); break;
            case "Table": throw new Error(`Not yet implemented`);
            case "Memory": throw new Error(`Not yet implemented`);
            case "Global": throw new Error(`Not yet implemented`);
            }
        }
    },

    Function: (section, bin) => {
        put(bin, "varuint32", section.data.length);
        for (const signature of section.data)
            put(bin, "varuint32", signature);
    },

    Table: (section, bin) => { throw new Error(`Not yet implemented`); },

    Memory: (section, bin) => {
        // Flags, currently can only be [0,1]
        put(bin, "varuint1", section.data.length);
        for (const memory of section.data) {
            put(bin, "varuint32", memory.max ? 1 : 0);
            put(bin, "varuint32", memory.initial);
            if (memory.max)
                put(bin, "varuint32", memory.max);
        }
    },

    Global: (section, bin) => { throw new Error(`Not yet implemented`); },
    Export: (section, bin) => {
        put(bin, "varuint32", section.data.length);
        for (const entry of section.data) {
            put(bin, "string", entry.field);
            put(bin, "uint8", WASM.externalKindValue[entry.kind]);
            switch (entry.kind) {
            default: throw new Error(`Implementation problem: unexpected kind ${entry.kind}`);
            case "Function": put(bin, "varuint32", entry.index); break;
            case "Table": throw new Error(`Not yet implemented`);
            case "Memory": throw new Error(`Not yet implemented`);
            case "Global": throw new Error(`Not yet implemented`);
            }
        }
    },
    Start: (section, bin) => { throw new Error(`Not yet implemented`); },
    Element: (section, bin) => { throw new Error(`Not yet implemented`); },

    Code: (section, bin) => {
        put(bin, "varuint32", section.data.length);
        for (const func of section.data) {
            let funcBin = bin.newPatchable("varuint32");
            const localCount = func.locals.length - func.parameterCount;
            put(funcBin, "varuint32", localCount);
            for (let i = func.parameterCount; i < func.locals.length; ++i) {
                put(funcBin, "varuint32", 1);
                put(funcBin, "uint8", WASM.valueTypeValue[func.locals[i]]);
            }

            for (const op of func.code) {
                put(funcBin, "uint8", op.value);
                if (op.arguments.length !== 0)
                    throw new Error(`Unimplemented: arguments`); // FIXME https://bugs.webkit.org/show_bug.cgi?id=162706

                switch (op.name) {
                default:
                    for (let i = 0; i < op.immediates.length; ++i) {
                        const type = WASM.description.opcode[op.name].immediate[i].type
                        if (!funcBin[type])
                            throw new TypeError(`Unknown type: ${type} in op: ${op.name}`);
                        put(funcBin, type, op.immediates[i]);
                    }
                    break;
                case "br_table":
                    put(funcBin, "varuint32", op.immediates.length - 1);
                    for (let imm of op.immediates)
                        put(funcBin, "varuint32", imm);
                    break;
                }
            }
            funcBin.apply();
        }
    },

    Data: (section, bin) => { throw new Error(`Not yet implemented`); },
};

export const Binary = (preamble, sections) => {
    let wasmBin = new LowLevelBinary();
    for (const p of WASM.description.preamble)
        put(wasmBin, p.type, preamble[p.name]);
    for (const section of sections) {
        put(wasmBin, WASM.sectionEncodingType, section.id);
        let sectionBin = wasmBin.newPatchable("varuint32");
        const emitter = emitters[section.name];
        if (emitter)
            emitter(section, sectionBin);
        else {
            // Unknown section.
            put(sectionBin, "string", section.name);
            for (const byte of section.data)
                put(sectionBin, "uint8", byte);
        }
        sectionBin.apply();
    }
    wasmBin.trim();
    return wasmBin;
};
