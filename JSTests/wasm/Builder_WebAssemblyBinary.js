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

import * as assert from './assert.js';
import LowLevelBinary from './LowLevelBinary.js';
import * as WASM from './WASM.js';

const put = (bin, type, value) => bin[type](value);

const putResizableLimits = (bin, initial, maximum) => {
    assert.truthy(typeof initial === "number", "We expect 'initial' to be an integer");
    let hasMaximum = 0;
    if (typeof maximum === "number") {
        hasMaximum = 1;
    } else {
        assert.truthy(typeof maximum === "undefined", "We expect 'maximum' to be an integer if it's defined");
    }

    put(bin, "varuint1", hasMaximum);
    put(bin, "varuint32", initial);
    if (hasMaximum)
        put(bin, "varuint32", maximum);
};

const putTable = (bin, {initial, maximum, element}) => {
    assert.truthy(WASM.isValidType(element), "We expect 'element' to be a valid type. It was: " + element);
    put(bin, "varint7", WASM.typeValue[element]);

    putResizableLimits(bin, initial, maximum);
};

const valueType = WASM.description.type.i32.type

const putGlobalType = (bin, global) => {
    put(bin, valueType, WASM.typeValue[global.type]);
    put(bin, "varuint1", global.mutability);
};

const putOp = (bin, op) => {
    put(bin, "uint8", op.value);
    if (WASM.description.opcode[op.name].extendedOp)
        put(bin, "uint8", WASM.description.opcode[op.name].extendedOp);
    if (op.arguments.length !== 0)
        throw new Error(`Unimplemented: arguments`); // FIXME https://bugs.webkit.org/show_bug.cgi?id=162706

    switch (op.name) {
    default:
        for (let i = 0; i < op.immediates.length; ++i) {
            const type = WASM.description.opcode[op.name].immediate[i].type
            if (!bin[type])
                throw new TypeError(`Unknown type: ${type} in op: ${op.name}`);
            put(bin, type, op.immediates[i]);
        }
        break;
    case "i32.const": {
        assert.eq(op.immediates.length, 1);
        let imm = op.immediates[0];
        // Do a static cast to make large int32s signed.
        if (imm >= 2 ** 31)
            imm = imm - (2 ** 32);
        put(bin, "varint32", imm);
        break;
    }
    case "br_table":
        put(bin, "varuint32", op.immediates.length - 1);
        for (let imm of op.immediates)
            put(bin, "varuint32", imm);
        break;
    }
};

const putInitExpr = (bin, expr) => {
    if (expr.op == "ref.null")
        putOp(bin, { value: WASM.description.opcode[expr.op].value, name: expr.op, immediates: [], arguments: [] });
    else
        putOp(bin, { value: WASM.description.opcode[expr.op].value, name: expr.op, immediates: [expr.initValue], arguments: [] });
    putOp(bin, { value: WASM.description.opcode.end.value, name: "end", immediates: [], arguments: [] });
};

const emitters = {
    Type: (section, bin) => {
        put(bin, "varuint32", section.data.length);
        for (const entry of section.data) {
            put(bin, "varint7", WASM.typeValue["func"]);
            put(bin, "varuint32", entry.params.length);
            for (const param of entry.params)
                put(bin, "varint7", WASM.typeValue[param]);
            if (entry.ret === "void")
                put(bin, "varuint1", 0);
            else {
                put(bin, "varuint1", 1);
                put(bin, "varint7", WASM.typeValue[entry.ret]);
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
            case "Function": {
                put(bin, "varuint32", entry.type);
                break;
            }
            case "Table": {
                putTable(bin, entry.tableDescription);
                break;
            }
            case "Memory": {
                let {initial, maximum} = entry.memoryDescription;
                putResizableLimits(bin, initial, maximum);
                break;
            };
            case "Global":
                putGlobalType(bin, entry.globalDescription);
                break;
            }
        }
    },

    Function: (section, bin) => {
        put(bin, "varuint32", section.data.length);
        for (const signature of section.data)
            put(bin, "varuint32", signature);
    },

    Table: (section, bin) => {
        put(bin, "varuint32", section.data.length);
        for (const {tableDescription} of section.data) {
            putTable(bin, tableDescription);
        }
    },

    Memory: (section, bin) => {
        // Flags, currently can only be [0,1]
        put(bin, "varuint1", section.data.length);
        for (const memory of section.data)
            putResizableLimits(bin, memory.initial, memory.maximum);
    },

    Global: (section, bin) => {
        put(bin, "varuint32", section.data.length);
        for (const global of section.data) {
            putGlobalType(bin, global);
            putInitExpr(bin, global)
        }
    },

    Export: (section, bin) => {
        put(bin, "varuint32", section.data.length);
        for (const entry of section.data) {
            put(bin, "string", entry.field);
            put(bin, "uint8", WASM.externalKindValue[entry.kind]);
            switch (entry.kind) {
            case "Global":
            case "Function":
            case "Memory":
            case "Table":
                put(bin, "varuint32", entry.index);
                break;
            default: throw new Error(`Implementation problem: unexpected kind ${entry.kind}`);
            }
        }
    },
    Start: (section, bin) => {
        put(bin, "varuint32", section.data[0]);
    },
    Element: (section, bin) => {
        const data = section.data;
        put(bin, "varuint32", data.length);
        for (const {tableIndex, offset, functionIndices} of data) {
            if (tableIndex != 0)
                put(bin, "uint8", 2);
            put(bin, "varuint32", tableIndex);

            let initExpr;
            if (typeof offset === "number")
                initExpr = {op: "i32.const", initValue: offset};
            else
                initExpr = offset;
            putInitExpr(bin, initExpr);

            put(bin, "varuint32", functionIndices.length);
            for (const functionIndex of functionIndices)
                put(bin, "varuint32", functionIndex);
        }
    },

    Code: (section, bin) => {
        put(bin, "varuint32", section.data.length);
        for (const func of section.data) {
            let funcBin = bin.newPatchable("varuint32");
            const localCount = func.locals.length - func.parameterCount;
            put(funcBin, "varuint32", localCount);
            for (let i = func.parameterCount; i < func.locals.length; ++i) {
                put(funcBin, "varuint32", 1);
                put(funcBin, "varint7", WASM.typeValue[func.locals[i]]);
            }

            for (const op of func.code)
                putOp(funcBin, op);

            funcBin.apply();
        }
    },

    Data: (section, bin) => {
        put(bin, "varuint32", section.data.length);
        for (const datum of section.data) {
            put(bin, "varuint32", datum.index);
            // FIXME allow complex init_expr here. https://bugs.webkit.org/show_bug.cgi?id=165700
            // For now we only handle i32.const as offset.
            put(bin, "uint8", WASM.description.opcode["i32.const"].value);
            put(bin, WASM.description.opcode["i32.const"].immediate[0].type, datum.offset);
            put(bin, "uint8", WASM.description.opcode["end"].value);
            put(bin, "varuint32", datum.data.length);
            for (const byte of datum.data)
                put(bin, "uint8", byte);
        }
    },
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
