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

import LowLevelBinary from 'LowLevelBinary.js';
import * as WASM from 'WASM.js';

const put = (bin, type, value) => bin[type](value);

const emitters = {
    Type: (section, bin) => {
        put(bin, "varuint", section.data.length);
        for (const entry of section.data) {
            const funcTypeConstructor = -0x20; // FIXME Move this to wasm.json.
            put(bin, "varint7", funcTypeConstructor);
            put(bin, "varuint", entry.params.length);
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
        put(bin, "varuint", section.data.length);
        for (const entry of section.data) {
            put(bin, "string", entry.module);
            put(bin, "string", entry.field);
            put(bin, "uint8", WASM.externalKindValue[entry.kind]);
            switch (entry.kind) {
            default: throw new Error(`Implementation problem: unexpected kind ${entry.kind}`);
            case "Function": put(bin, "varuint", entry.type); break;
            case "Table": throw new Error(`Not yet implemented`);
            case "Memory": throw new Error(`Not yet implemented`);
            case "Global": throw new Error(`Not yet implemented`);
            }
        }
    },
    Function: (section, bin) => { throw new Error(`Not yet implemented`); },
    Table: (section, bin) => { throw new Error(`Not yet implemented`); },
    Memory: (section, bin) => { throw new Error(`Not yet implemented`); },
    Global: (section, bin) => { throw new Error(`Not yet implemented`); },
    Export: (section, bin) => { throw new Error(`Not yet implemented`); },
    Start: (section, bin) => { throw new Error(`Not yet implemented`); },
    Element: (section, bin) => { throw new Error(`Not yet implemented`); },
    Code: (section, bin) => {
        put(bin, "varuint", section.data.length);
        for (const func of section.data) {
            let funcBin = bin.newPatchable("varuint");
            const localCount = func.locals.length;
            put(funcBin, "varuint", localCount);
            if (localCount !== 0) throw new Error(`Unimplemented: locals`); // FIXME https://bugs.webkit.org/show_bug.cgi?id=162706
            for (const op of func.code) {
                put(funcBin, "uint8", op.value);
                if (op.arguments.length !== 0) throw new Error(`Unimplemented: arguments`); // FIXME https://bugs.webkit.org/show_bug.cgi?id=162706
                if (op.immediates.length !== 0) throw new Error(`Unimplemented: immediates`); // FIXME https://bugs.webkit.org/show_bug.cgi?id=162706
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
        let sectionBin = wasmBin.newPatchable("varuint");
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
    return wasmBin;
};
