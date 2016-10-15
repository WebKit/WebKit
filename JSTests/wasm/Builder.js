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

const _toJavaScriptName = name => {
    const camelCase = name.replace(/([^a-z0-9].)/g, c => c[1].toUpperCase());
    const CamelCase = camelCase.charAt(0).toUpperCase() + camelCase.slice(1);
    return CamelCase;
};

const _isValidValue = (value, type) => {
    switch (type) {
    case "i32": return ((value & 0xFFFFFFFF) >>> 0) === value;
    case "i64": throw new Error(`Unimplemented: value check for ${type}`); // FIXME https://bugs.webkit.org/show_bug.cgi?id=163420 64-bit values
    case "f32": return typeof(value) === "number" && isFinite(value);
    case "f64": return typeof(value) === "number" && isFinite(value);
    default: throw new Error(`Implementation problem: unknown type ${type}`);
    }
};
const _unknownSectionId = 0;

const _BuildWebAssemblyBinary = (preamble, sections) => {
    let wasmBin = new LowLevelBinary();
    const put = (bin, type, value) => bin[type](value);
    for (const p of WASM.description.preamble)
        put(wasmBin, p.type, preamble[p.name]);
    for (const section of sections) {
        put(wasmBin, WASM.sectionEncodingType, section.id);
        let sectionBin = wasmBin.newPatchable("varuint");
        switch (section.name) {
        case "Type": throw new Error(`Unimplemented: section type "${section.name}"`);
        case "Import": throw new Error(`Unimplemented: section type "${section.name}"`);
        case "Function": throw new Error(`Unimplemented: section type "${section.name}"`);
        case "Table": throw new Error(`Unimplemented: section type "${section.name}"`);
        case "Memory": throw new Error(`Unimplemented: section type "${section.name}"`);
        case "Global": throw new Error(`Unimplemented: section type "${section.name}"`);
        case "Export": throw new Error(`Unimplemented: section type "${section.name}"`);
        case "Start": throw new Error(`Unimplemented: section type "${section.name}"`);
        case "Element": throw new Error(`Unimplemented: section type "${section.name}"`);
        case "Code":
            const numberOfFunctionBodies = section.data.length;
            put(sectionBin, "varuint", numberOfFunctionBodies);
            for (const func of section.data) {
                let funcBin = sectionBin.newPatchable("varuint");
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
            break;
        case "Data": throw new Error(`Unimplemented: section type "${section.name}"`);
        default:
            if (section.id !== _unknownSectionId) throw new Error(`Unknown section "${section.name}" with number ${section.id}`);
            put(sectionBin, "string", section.name);
            for (const byte of section.data)
                put(sectionBin, "uint8", byte);
            break;
        }
        sectionBin.apply();
    }
    return wasmBin;
};

export default class Builder {
    constructor() {
        this.setChecked(true);
        let preamble = {};
        for (const p of WASM.description.preamble)
            preamble[p.name] = p.value;
        this.setPreamble(preamble);
        this._sections = [];
        this._registerSectionBuilders();
    }
    setChecked(checked) {
        this._checked = checked;
        return this;
    }
    setPreamble(p) {
        this._preamble = Object.assign(this._preamble || {}, p);
        return this;
    }
    _registerSectionBuilders() {
        for (const section in WASM.description.section) {
            switch (section) {
            case "Code":
                this[section] = function() {
                    const s = this._addSection(section);
                    const builder = this;
                    const codeBuilder =  {
                        End: () => builder,
                        Function: parameters => {
                            parameters = parameters || [];
                            const invalidParameterTypes = parameters.filter(p => !WASM.isValidValueType(p));
                            if (invalidParameterTypes.length !== 0) throw new Error(`Function declared with parameters [${parameters}], invalid: [${invalidParameterTypes}]`);
                            const func = {
                                locals: parameters, // Parameters are the first locals.
                                parameterCount: parameters.length,
                                code: []
                            };
                            s.data.push(func);
                            let functionBuilder = {};
                            for (const op in WASM.description.opcode) {
                                const name = _toJavaScriptName(op);
                                const value = WASM.description.opcode[op].value;
                                const ret = WASM.description.opcode[op]["return"];
                                const param = WASM.description.opcode[op].parameter;
                                const imm = WASM.description.opcode[op].immediate;
                                const checkStackArgs = builder._checked ? op => {
                                    for (let expect of param) {
                                        if (WASM.isValidValueType(expect)) {
                                            // FIXME implement stack checks for arguments. https://bugs.webkit.org/show_bug.cgi?id=163421
                                        } else {
                                            // Handle our own meta-types.
                                            switch (expect) {
                                            case "addr": break; // FIXME implement addr. https://bugs.webkit.org/show_bug.cgi?id=163421
                                            case "any": break; // FIXME implement any. https://bugs.webkit.org/show_bug.cgi?id=163421
                                            case "bool": break; // FIXME implement bool. https://bugs.webkit.org/show_bug.cgi?id=163421
                                            case "call": break; // FIXME implement call stack argument checks based on function signature. https://bugs.webkit.org/show_bug.cgi?id=163421
                                            case "global": break; // FIXME implement global. https://bugs.webkit.org/show_bug.cgi?id=163421
                                            case "local": break; // FIXME implement local. https://bugs.webkit.org/show_bug.cgi?id=163421
                                            case "prev": break; // FIXME implement prev, checking for whetever the previous value was. https://bugs.webkit.org/show_bug.cgi?id=163421
                                            case "size": break; // FIXME implement size. https://bugs.webkit.org/show_bug.cgi?id=163421
                                            default: throw new Error(`Implementation problem: unhandled meta-type "${expect}" on "${op}"`);
                                            }
                                        }
                                    }
                                } : () => {};
                                const checkStackReturn = builder._checked ? op => {
                                    for (let expect of ret) {
                                        if (WASM.isValidValueType(expect)) {
                                            // FIXME implement stack checks for return. https://bugs.webkit.org/show_bug.cgi?id=163421
                                        } else {
                                            // Handle our own meta-types.
                                            switch (expect) {
                                            case "bool": break; // FIXME implement bool. https://bugs.webkit.org/show_bug.cgi?id=163421
                                            case "call": break; // FIXME implement call stack return check based on function signature. https://bugs.webkit.org/show_bug.cgi?id=163421
                                            case "control": break; // FIXME implement control. https://bugs.webkit.org/show_bug.cgi?id=163421
                                            case "global": break; // FIXME implement global. https://bugs.webkit.org/show_bug.cgi?id=163421
                                            case "local": break; // FIXME implement local. https://bugs.webkit.org/show_bug.cgi?id=163421
                                            case "prev": break; // FIXME implement prev, checking for whetever the parameter type was. https://bugs.webkit.org/show_bug.cgi?id=163421
                                            case "size": break; // FIXME implement size. https://bugs.webkit.org/show_bug.cgi?id=163421
                                            default: throw new Error(`Implementation problem: unhandled meta-type "${expect}" on "${op}"`);
                                            }
                                        }
                                    }
                                } : () => {};
                                const checkImms = builder._checked ? (op, imms) => {
                                    if (imms.length != imm.length) throw new Error(`"${op}" expects ${imm.length} immediates, got ${imms.length}`);
                                    for (let idx = 0; idx !== imm.length; ++idx) {
                                        const got = imms[idx];
                                        const expect = imm[idx];
                                        switch (expect.name) {
                                        case "function_index":
                                            if (!_isValidValue(got, "i32")) throw new Error(`Invalid value on ${op}: got "${got}", expected i32`);
                                            // FIXME check function indices. https://bugs.webkit.org/show_bug.cgi?id=163421
                                            break;
                                        case "local_index": throw new Error(`Unimplemented: "${expect.name}" on "${op}"`);
                                        case "global_index": throw new Error(`Unimplemented: "${expect.name}" on "${op}"`);
                                        case "type_index": throw new Error(`Unimplemented: "${expect.name}" on "${op}"`);
                                        case "value": if (!_isValidValue(got, ret[0])) throw new Error(`Invalid value on ${op}: got "${got}", expected ${ret[0]}`); break;
                                        case "flags": throw new Error(`Unimplemented: "${expect.name}" on "${op}"`);
                                        case "offset": throw new Error(`Unimplemented: "${expect.name}" on "${op}"`);
                                        // Control:
                                        case "default_target": throw new Error(`Unimplemented: "${expect.name}" on "${op}"`);
                                        case "relative_depth": throw new Error(`Unimplemented: "${expect.name}" on "${op}"`);
                                        case "sig": throw new Error(`Unimplemented: "${expect.name}" on "${op}"`);
                                        case "target_count": throw new Error(`Unimplemented: "${expect.name}" on "${op}"`);
                                        case "target_table": throw new Error(`Unimplemented: "${expect.name}" on "${op}"`);
                                        default: throw new Error(`Implementation problem: unhandled immediate "${expect.name}" on "${op}"`);
                                        }
                                    }
                                } : () => {};
                                const nextBuilder = name === "End" ? codeBuilder : functionBuilder;
                                functionBuilder[name] = (...args) => {
                                    const imms = args; // FIXME: allow passing in stack values, as-if code were a stack machine. Just check for a builder to this function, and drop. https://bugs.webkit.org/show_bug.cgi?id=163422
                                    checkImms(op, imms);
                                    checkStackArgs(op);
                                    checkStackReturn(op);
                                    const stackArgs = []; // FIXME https://bugs.webkit.org/show_bug.cgi?id=162706
                                    func.code.push({ name: op, value: value, arguments: stackArgs, immediates: imms });
                                    return nextBuilder;
                                };
                            }
                            return functionBuilder;
                        }
                    };
                    return codeBuilder;
                };
                break;
            default:
                this[section] = () => { throw new Error(`Unimplemented: section type "${section}"`); };
                break;
            }
        }
        this.Unknown = function(name) {
            const s = this._addSection(name);
            const builder = this;
            const unknownBuilder =  {
                End: () => builder,
                Byte: b => { if ((b & 0xFF) !== b) throw new RangeError(`Unknown section expected byte, got: "${b}"`); s.data.push(b); return unknownBuilder; }
            };
            return unknownBuilder;
        };
    }
    _addSection(nameOrNumber, extraObject) {
        const name = typeof(nameOrNumber) === "string" ? nameOrNumber : "";
        const number = typeof(nameOrNumber) === "number" ? nameOrNumber : (WASM.description.section[name] ? WASM.description.section[name].value : _unknownSectionId);
        const s = Object.assign({ name: name, id: number, data: [] }, extraObject || {});
        this._sections.push(s);
        return s;
    }
    optimize() {
        // FIXME Add more optimizations. https://bugs.webkit.org/show_bug.cgi?id=163424
        return this;
    }
    json() {
        const obj = {
            preamble: this._preamble,
            section: this._sections
        };
        return JSON.stringify(obj);
    }
    AsmJS() {
        "use asm"; // For speed.
        // FIXME Create an asm.js equivalent string which can be eval'd. https://bugs.webkit.org/show_bug.cgi?id=163425
        throw new Error("asm.js not implemented yet");
    }
    WebAssembly() { return _BuildWebAssemblyBinary(this._preamble, this._sections); }
};
