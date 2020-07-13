/*
 * Copyright (C) 2016-2017 Apple Inc. All rights reserved.
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

import * as utilities from './utilities.js';

const _mapValues = from => {
    let values = {};
    for (const key in from)
        values[key] = from[key].value;
    return values;
};

export const description = utilities.json("wasm.json");
export const type = Object.keys(description.type);
const _typeSet = new Set(type);
export const isValidType = v => _typeSet.has(v);
export const typeValue = _mapValues(description.type);
const _valueTypeSet = new Set(description.value_type);
export const isValidValueType = v => _valueTypeSet.has(v);
const _blockTypeSet = new Set(description.block_type);
export const isValidBlockType = v => _blockTypeSet.has(v);
export const externalKindValue = _mapValues(description.external_kind);
export const sections = Object.keys(description.section);
export const sectionEncodingType = description.section[sections[0]].type;

export function* opcodes(category = undefined) {
    for (let op in description.opcode)
        if (category !== undefined && description.opcode[op].category === category)
            yield { name: op, opcode: description.opcode[op] };
};
export const memoryAccessInfo = op => {
    //                <-----------valueType----------->  <-------type-------><---------width-------->  <--sign-->
    const classify = /((?:i32)|(?:i64)|(?:f32)|(?:f64))\.((?:load)|(?:store))((?:8)|(?:16)|(?:32))?_?((?:s|u)?)/;
    const found = op.name.match(classify);
    const valueType = found[1];
    const type = found[2];
    const width = parseInt(found[3] ? found[3] : valueType.slice(1));
    const sign = (() => {
        switch (found[4]) {
        case "s": return "signed";
        case "u": return "unsigned";
        default: return "agnostic";
        }
    })();
    return { valueType, type, width, sign };
};

export const constForValueType = valueType => {
    for (let op in description.opcode)
        if (op.endsWith(".const") && description.opcode[op]["return"] == valueType)
            return op;
    throw new Error(`Implementation problem: no const type for ${valueType}`);
};

