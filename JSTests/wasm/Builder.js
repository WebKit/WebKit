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
import * as BuildWebAssembly from 'Builder_WebAssemblyBinary.js';
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

const _normalizeFunctionSignature = (params, ret) => {
    assert.isArray(params);
    for (const p of params)
        assert.truthy(WASM.isValidValueType(p), `Type parameter ${p} needs a valid value type`);
    if (typeof(ret) === "undefined")
        ret = "void";
    assert.isNotArray(ret, `Multiple return values not supported by WebAssembly yet`);
    assert.truthy(WASM.isValidBlockType(ret), `Type return ${ret} must be valid block type`);
    return [params, ret];
};

const _maybeRegisterType = (builder, type) => {
    const typeSection = builder._getSection("Type");
    if (typeof(type) === "number") {
        // Type numbers already refer to the type section, no need to register them.
        if (builder._checked) {
            assert.isNotUndef(typeSection, `Can not use type ${type} if a type section is not present`);
            assert.isNotUndef(typeSection.data[type], `Type ${type} does not exist in type section`);
        }
        return type;
    }
    assert.hasObjectProperty(type, "params", `Expected type to be a number or object with 'params' and optionally 'ret' fields`);
    const [params, ret] = _normalizeFunctionSignature(type.params, type.ret);
    assert.isNotUndef(typeSection, `Can not add type if a type section is not present`);
    // Try reusing an equivalent type from the type section.
    types:
    for (let i = 0; i !== typeSection.data.length; ++i) {
        const t = typeSection.data[i];
        if (t.ret === ret && params.length === t.params.length) {
            for (let j = 0; j !== t.params.length; ++j) {
                if (params[j] !== t.params[j])
                    continue types;
            }
            type = i;
            break;
        }
    }
    if (typeof(type) !== "number") {
        // Couldn't reuse a pre-existing type, register this type in the type section.
        typeSection.data.push({ params: params, ret: ret });
        type = typeSection.data.length - 1;
    }
    return type;
};

const _importFunctionContinuation = (builder, section, nextBuilder) => {
    return (module, field, type) => {
        assert.isString(module, `Import function module should be a string, got "${module}"`);
        assert.isString(field, `Import function field should be a string, got "${field}"`);
        const typeSection = builder._getSection("Type");
        type = _maybeRegisterType(builder, type);
        section.data.push({ field: field, type: type, kind: "Function", module: module });
        // Imports also count in the function index space. Map them as objects to avoid clashing with Code functions' names.
        builder._registerFunctionToIndexSpace({ module: module, field: field });
        return nextBuilder;
    };
};

const _importMemoryContinuation = (builder, section, nextBuilder) => {
    return (module, field, {initial, maximum}) => {
        assert.isString(module, `Import Memory module should be a string, got "${module}"`);
        assert.isString(field, `Import Memory field should be a string, got "${field}"`);
        section.data.push({module, field, kind: "Memory", memoryDescription: {initial, maximum}});
        return nextBuilder;
    };
};

const _importTableContinuation = (builder, section, nextBuilder) => {
    return (module, field, {initial, maximum, element}) => {
        assert.isString(module, `Import Table module should be a string, got "${module}"`);
        assert.isString(field, `Import Table field should be a string, got "${field}"`);
        section.data.push({module, field, kind: "Table", tableDescription: {initial, maximum, element}});
        return nextBuilder;
    };
};

const _exportFunctionContinuation = (builder, section, nextBuilder) => {
    return (field, index, type) => {
        assert.isString(field, `Export function field should be a string, got "${field}"`);
        const typeSection = builder._getSection("Type");
        if (typeof(type) !== "undefined") {
            // Exports can leave the type unspecified, letting the Code builder patch them up later.
            type = _maybeRegisterType(builder, type);
        }

        // We can't check much about "index" here because the Code section succeeds the Export section. More work is done at Code().End() time.
        switch (typeof(index)) {
        case "string": break; // Assume it's a function name which will be revealed in the Code section.
        case "number": break; // Assume it's a number in the "function index space".
        case "object":
            // Re-exporting an import.
            assert.hasObjectProperty(index, "module", `Re-exporting "${field}" from an import`);
            assert.hasObjectProperty(index, "field", `Re-exporting "${field}" from an import`);
            break;
        case "undefined":
            // Assume it's the same as the field (i.e. it's not being renamed).
            index = field;
            break;
        default: throw new Error(`Export section's index must be a string or a number, got ${index}`);
        }

        const correspondingImport = builder._getFunctionFromIndexSpace(index);
        const importSection = builder._getSection("Import");
        if (typeof(index) === "object") {
            // Re-exporting an import using its module+field name.
            assert.isNotUndef(correspondingImport, `Re-exporting "${field}" couldn't find import from module "${index.module}" field "${index.field}"`);
            index = correspondingImport;
            if (typeof(type) === "undefined")
                type = importSection.data[index].type;
            if (builder._checked)
                assert.eq(type, importSection.data[index].type, `Re-exporting import "${importSection.data[index].field}" as "${field}" has mismatching type`);
        } else if (typeof(correspondingImport) !== "undefined") {
            // Re-exporting an import using its index.
            let exportedImport;
            for (const i of importSection.data) {
                if (i.module === correspondingImport.module && i.field === correspondingImport.field) {
                    exportedImport = i;
                    break;
                }
            }
            if (typeof(type) === "undefined")
                type = exportedImport.type;
            if (builder._checked)
                assert.eq(type, exportedImport.type, `Re-exporting import "${exportedImport.field}" as "${field}" has mismatching type`);
        }
        section.data.push({ field: field, type: type, kind: "Function", index: index });
        return nextBuilder;
    };
};

const _checkStackArgs = (op, param) => {
    for (let expect of param) {
        if (WASM.isValidType(expect)) {
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
}

const _checkStackReturn = (op, ret) => {
    for (let expect of ret) {
        if (WASM.isValidType(expect)) {
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
};

const _checkImms = (op, imms, expectedImms, ret) => {
    const minExpectedImms = expectedImms.filter(i => i.type.slice(-1) !== '*').length;
    if (minExpectedImms !== expectedImms.length)
        assert.ge(imms.length, minExpectedImms, `"${op}" expects at least ${minExpectedImms} immediate${minExpectedImms !== 1 ? 's' : ''}, got ${imms.length}`);
    else
         assert.eq(imms.length, minExpectedImms, `"${op}" expects exactly ${minExpectedImms} immediate${minExpectedImms !== 1 ? 's' : ''}, got ${imms.length}`);
    for (let idx = 0; idx !== expectedImms.length; ++idx) {
        const got = imms[idx];
        const expect = expectedImms[idx];
        switch (expect.name) {
        case "function_index":
            assert.truthy(_isValidValue(got, "i32"), `Invalid value on ${op}: got "${got}", expected i32`);
            // FIXME check function indices. https://bugs.webkit.org/show_bug.cgi?id=163421
            break;
        case "local_index": break; // improve checking https://bugs.webkit.org/show_bug.cgi?id=163421
        case "global_index": break; // improve checking https://bugs.webkit.org/show_bug.cgi?id=163421
        case "type_index": break; // improve checking https://bugs.webkit.org/show_bug.cgi?id=163421
        case "value":
            assert.truthy(_isValidValue(got, ret[0]), `Invalid value on ${op}: got "${got}", expected ${ret[0]}`);
            break;
        case "flags": break; // improve checking https://bugs.webkit.org/show_bug.cgi?id=163421
        case "offset": break; // improve checking https://bugs.webkit.org/show_bug.cgi?id=163421
            // Control:
        case "default_target": break; // improve checking https://bugs.webkit.org/show_bug.cgi?id=163421
        case "relative_depth": break; // improve checking https://bugs.webkit.org/show_bug.cgi?id=163421
        case "sig":
            assert.truthy(WASM.isValidBlockType(imms[idx]), `Invalid block type on ${op}: "${imms[idx]}"`);
            break;
        case "target_count": break; // improve checking https://bugs.webkit.org/show_bug.cgi?id=163421
        case "target_table": break; // improve checking https://bugs.webkit.org/show_bug.cgi?id=163421
        case "reserved": break; // improve checking https://bugs.webkit.org/show_bug.cgi?id=163421
        default: throw new Error(`Implementation problem: unhandled immediate "${expect.name}" on "${op}"`);
        }
    }
};

const _createFunctionBuilder = (func, builder, previousBuilder) => {
    let functionBuilder = {};
    for (const op in WASM.description.opcode) {
        const name = _toJavaScriptName(op);
        const value = WASM.description.opcode[op].value;
        const ret = WASM.description.opcode[op]["return"];
        const param = WASM.description.opcode[op].parameter;
        const imm = WASM.description.opcode[op].immediate;

        const checkStackArgs = builder._checked ? _checkStackArgs : () => {};
        const checkStackReturn = builder._checked ? _checkStackReturn : () => {};
        const checkImms = builder._checked ? _checkImms : () => {};

        functionBuilder[name] = (...args) => {
            let nextBuilder;
            switch (name) {
            default:
                nextBuilder = functionBuilder;
                break;
                case "End":
                nextBuilder = previousBuilder;
                    break;
            case "Block":
                case "Loop":
            case "If":
                nextBuilder = _createFunctionBuilder(func, builder, functionBuilder);
                break;
            }

            // Passing a function as the argument is a way to nest blocks lexically.
            const continuation = args[args.length - 1];
            const hasContinuation = typeof(continuation) === "function";
            const imms = hasContinuation ? args.slice(0, -1) : args; // FIXME: allow passing in stack values, as-if code were a stack machine. Just check for a builder to this function, and drop. https://bugs.webkit.org/show_bug.cgi?id=163422
            checkImms(op, imms, imm, ret);
            checkStackArgs(op, param);
            checkStackReturn(op, ret);
            const stackArgs = []; // FIXME https://bugs.webkit.org/show_bug.cgi?id=162706
            func.code.push({ name: op, value: value, arguments: stackArgs, immediates: imms });
            if (hasContinuation)
                return continuation(nextBuilder).End();
            return nextBuilder;
        };
    }
    return functionBuilder;
}

const _createFunction = (section, builder, previousBuilder) => {
    return (...args) => {
        const nameOffset = (typeof(args[0]) === "string") >>> 0;
        const functionName = nameOffset ? args[0] : undefined;
        let signature = args[0 + nameOffset];
        const locals = args[1 + nameOffset] === undefined ? [] : args[1 + nameOffset];
        for (const local of locals)
            assert.truthy(WASM.isValidValueType(local), `Type of local: ${local} needs to be a valid value type`);

        if (typeof(signature) === "undefined")
            signature = { params: [] };

        let type;
        let params;
        if (typeof signature === "object") {
            assert.hasObjectProperty(signature, "params", `Expect function signature to be an object with a "params" field, got "${signature}"`);
            let ret;
            ([params, ret] = _normalizeFunctionSignature(signature.params, signature.ret));
            signature = {params, ret};
            type = _maybeRegisterType(builder, signature);
        } else {
            assert.truthy(typeof signature === "number");
            const typeSection = builder._getSection("Type");
            assert.truthy(!!typeSection);
            // FIXME: we should allow indices that exceed this to be able to
            // test JSCs validator is correct. https://bugs.webkit.org/show_bug.cgi?id=165786
            assert.truthy(signature < typeSection.data.length);
            type = signature;
            signature = typeSection.data[signature];
            assert.hasObjectProperty(signature, "params", `Expect function signature to be an object with a "params" field, got "${signature}"`);
            params = signature.params;
        }

        const func = {
            name: functionName,
            type,
            signature: signature,
            locals: params.concat(locals), // Parameters are the first locals.
            parameterCount: params.length,
            code: []
        };

        const functionSection = builder._getSection("Function");
        if (functionSection)
            functionSection.data.push(func.type);

        section.data.push(func);
        builder._registerFunctionToIndexSpace(functionName);

        return _createFunctionBuilder(func, builder, previousBuilder);
    }
};

export default class Builder {
    constructor() {
        this.setChecked(true);
        let preamble = {};
        for (const p of WASM.description.preamble)
            preamble[p.name] = p.value;
        this.setPreamble(preamble);
        this._sections = [];
        this._functionIndexSpace = {};
        this._functionIndexSpaceCount = 0;
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
    _registerFunctionToIndexSpace(name) {
        if (typeof(name) === "undefined") {
            // Registering a nameless function still adds it to the function index space. Register it as something that can't normally be registered.
            name = {};
        }
        // Collisions are fine: we'll simply count the function and forget the previous one.
        this._functionIndexSpace[name] = this._functionIndexSpaceCount++;
        // Map it both ways, the number space is distinct from the name space.
        this._functionIndexSpace[this._functionIndexSpace[name]] = name;
    }
    _getFunctionFromIndexSpace(name) {
        return this._functionIndexSpace[name];
    }
    _registerSectionBuilders() {
        for (const section in WASM.description.section) {
            switch (section) {
            case "Type":
                this[section] = function() {
                    const s = this._addSection(section);
                    const builder = this;
                    const typeBuilder = {
                        End: () => builder,
                        Func: (params, ret) => {
                            [params, ret] = _normalizeFunctionSignature(params, ret);
                            s.data.push({ params: params, ret: ret });
                            return typeBuilder;
                        },
                    };
                    return typeBuilder;
                };
                break;

            case "Import":
                this[section] = function() {
                    const s = this._addSection(section);
                    const importBuilder = {
                        End: () => this,
                        Global: () => { throw new Error(`Unimplemented: import global`); },
                    };
                    importBuilder.Function = _importFunctionContinuation(this, s, importBuilder);
                    importBuilder.Memory = _importMemoryContinuation(this, s, importBuilder);
                    importBuilder.Table = _importTableContinuation(this, s, importBuilder);
                    return importBuilder;
                };
                break;

            case "Function":
                this[section] = function() {
                    const s = this._addSection(section);
                    const exportBuilder = {
                        End: () => this
                        // FIXME: add ability to add this with whatever.
                    };
                    return exportBuilder;
                };
                break;

            case "Table":
                this[section] = function() {
                    const s = this._addSection(section);
                    const exportBuilder = {
                        End: () => this,
                        Table: ({initial, maximum, element}) => {
                            s.data.push({tableDescription: {initial, maximum, element}});
                            return exportBuilder;
                        }
                    };
                    return exportBuilder;
                };
                break;

            case "Memory":
                this[section] = function() {
                    const s = this._addSection(section);
                    const exportBuilder = {
                        End: () => this,
                        InitialMaxPages: (initial, max) => {
                            s.data.push({ initial, max });
                            return exportBuilder;
                        }
                    };
                    return exportBuilder;
                };
                break;

            case "Global":
                // FIXME implement global https://bugs.webkit.org/show_bug.cgi?id=164133
                this[section] = () => { throw new Error(`Unimplemented: section type "${section}"`); };
                break;

            case "Export":
                this[section] = function() {
                    const s = this._addSection(section);
                    const exportBuilder = {
                        End: () => this,
                        Table: () => { throw new Error(`Unimplemented: export table`); },
                        Memory: () => { throw new Error(`Unimplemented: export memory`); },
                        Global: () => { throw new Error(`Unimplemented: export global`); },
                    };
                    exportBuilder.Function = _exportFunctionContinuation(this, s, exportBuilder);
                    return exportBuilder;
                };
                break;

            case "Start":
                this[section] = function(functionIndexOrName) {
                    const s = this._addSection(section);
                    const startBuilder = {
                        End: () => this,
                    };
                    if (typeof(functionIndexOrName) !== "number" && typeof(functionIndexOrName) !== "string")
                        throw new Error(`Start section's function index  must either be a number or a string`);
                    s.data.push(functionIndexOrName);
                    return startBuilder;
                };
                break;

            case "Element":
                // FIXME implement element https://bugs.webkit.org/show_bug.cgi?id=161709
                this[section] = () => { throw new Error(`Unimplemented: section type "${section}"`); };
                break;

            case "Code":
                this[section] = function() {
                    const s = this._addSection(section);
                    const builder = this;
                    const codeBuilder =  {
                        End: () => {
                            // We now have enough information to remap the export section's "type" and "index" according to the Code section we're currently ending.
                            const typeSection = builder._getSection("Type");
                            const importSection = builder._getSection("Import");
                            const exportSection = builder._getSection("Export");
                            const startSection = builder._getSection("Start");
                            const codeSection = s;
                            if (exportSection) {
                                for (const e of exportSection.data) {
                                    switch (typeof(e.index)) {
                                    default: throw new Error(`Unexpected export index "${e.index}"`);
                                    case "string": {
                                        const index = builder._getFunctionFromIndexSpace(e.index);
                                        assert.isNumber(index, `Export section contains undefined function "${e.index}"`);
                                        e.index = index;
                                    } // Fallthrough.
                                    case "number": {
                                        const index = builder._getFunctionFromIndexSpace(e.index);
                                        if (builder._checked)
                                            assert.isNotUndef(index, `Export "${e.field}" does not correspond to a defined value in the function index space`);
                                    } break;
                                    case "undefined":
                                        throw new Error(`Unimplemented: Function().End() with undefined export index`); // FIXME
                                    }
                                    if (typeof(e.type) === "undefined") {
                                        // This must be a function export from the Code section (re-exports were handled earlier).
                                        let functionIndexSpaceOffset = 0;
                                        if (importSection) {
                                            for (const {kind} of importSection.data) {
                                                if (kind === "Function")
                                                    ++functionIndexSpaceOffset;
                                            }
                                        }
                                        const functionIndex = e.index - functionIndexSpaceOffset;
                                        e.type = codeSection.data[functionIndex].type;
                                    }
                                }
                            }
                            if (startSection) {
                                const start = startSection.data[0];
                                let mapped = builder._getFunctionFromIndexSpace(start);
                                if (!builder._checked) {
                                    if (typeof(mapped) === "undefined")
                                        mapped = start; // In unchecked mode, simply use what was provided if it's nonsensical.
                                    assert.isA(start, "number"); // It can't be too nonsensical, otherwise we can't create a binary.
                                    startSection.data[0] = start;
                                } else {
                                    if (typeof(mapped) === "undefined")
                                        throw new Error(`Start section refers to non-existant function '${start}'`);
                                    if (typeof(start) === "string" || typeof(start) === "object")
                                        startSection.data[0] = mapped;
                                    // FIXME in checked mode, test that the type is acceptable for start function. We probably want _registerFunctionToIndexSpace to also register types per index. https://bugs.webkit.org/show_bug.cgi?id=165658
                                }
                            }
                            return builder;
                        },

                    };
                    codeBuilder.Function = _createFunction(s, builder, codeBuilder);
                    return codeBuilder;
                };
                break;

            case "Data":
                this[section] = function() {
                    const s = this._addSection(section);
                    const dataBuilder = {
                        End: () => this,
                        Segment: data => {
                            assert.isArray(data);
                            for (const datum of data) {
                                assert.isNumber(datum);
                                assert.ge(datum, 0);
                                assert.le(datum, 0xff);
                            }
                            s.data.push({ data: data, index: 0, offset: 0 });
                            let thisSegment = s.data[s.data.length - 1];
                            const segmentBuilder = {
                                End: () => dataBuilder,
                                Index: index => {
                                    assert.eq(index, 0); // Linear memory index must be zero in MVP.
                                    thisSegment.index = index;
                                    return segmentBuilder;
                                },
                                Offset: offset => {
                                    // FIXME allow complex init_expr here. https://bugs.webkit.org/show_bug.cgi?id=165700
                                    assert.isNumber(offset);
                                    thisSegment.offset = offset;
                                    return segmentBuilder;
                                },
                            };
                            return segmentBuilder;
                        },
                    };
                    return dataBuilder;
                };
                break;

            default:
                this[section] = () => { throw new Error(`Unknown section type "${section}"`); };
                break;
            }
        }

        this.Unknown = function(name) {
            const s = this._addSection(name);
            const builder = this;
            const unknownBuilder =  {
                End: () => builder,
                Byte: b => {
                    assert.eq(b & 0xFF, b, `Unknown section expected byte, got: "${b}"`);
                    s.data.push(b);
                    return unknownBuilder;
                }
            };
            return unknownBuilder;
        };
    }
    _addSection(nameOrNumber, extraObject) {
        const name = typeof(nameOrNumber) === "string" ? nameOrNumber : "";
        const number = typeof(nameOrNumber) === "number" ? nameOrNumber : (WASM.description.section[name] ? WASM.description.section[name].value : _unknownSectionId);
        if (this._checked) {
            // Check uniqueness.
            for (const s of this._sections)
                assert.falsy(s.name === name && s.id === number, `Cannot have two sections with the same name "${name}" and ID ${number}`);
            // Check ordering.
            if ((number !== _unknownSectionId) && (this._sections.length !== 0)) {
                for (let i = this._sections.length - 1; i >= 0; --i) {
                    if (this._sections[i].id === _unknownSectionId)
                        continue;
                    assert.le(this._sections[i].id, number, `Bad section ordering: "${this._sections[i].name}" cannot precede "${name}"`);
                    break;
                }
            }
        }
        const s = Object.assign({ name: name, id: number, data: [] }, extraObject || {});
        this._sections.push(s);
        return s;
    }
    _getSection(nameOrNumber) {
        switch (typeof(nameOrNumber)) {
        default: throw new Error(`Implementation problem: can not get section "${nameOrNumber}"`);
        case "string":
            for (const s of this._sections)
                if (s.name === nameOrNumber)
                    return s;
            return undefined;
        case "number":
            for (const s of this._sections)
                if (s.id === nameOrNumber)
                    return s;
            return undefined;
        }
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
    WebAssembly() { return BuildWebAssembly.Binary(this._preamble, this._sections); }
};
