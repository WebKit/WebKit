import * as assert from '../assert.js';
import * as WASM from '../WASM.js';

assert.isNotUndef(WASM.description);
assert.isNotUndef(WASM.type);
assert.ge(WASM.type.length, 7);

for (const v of WASM.type)
    if (!WASM.isValidType(v))
        throw new Error(`Expected value ${v} to be a valid type`);

const expectedFields = [
    "preamble",
    "type",
    "external_kind",
    "section",
    "opcode",
];
for (const e of expectedFields) {
    assert.isNotUndef(WASM.description[e]);
    if (typeof(WASM.description[e]) !== "object")
        throw new Error(`Expected description to contain field "${e}"`);
}

const expectedOpFields = [
    "category",
    "value",
    "return",
    "parameter",
    "immediate",
];
for (const op in WASM.description.opcode)
    for (const e of expectedOpFields)
        assert.isNotUndef(WASM.description.opcode[op][e]);

// FIXME: test for field "b3op" when all arithmetic/ comparison ops have them. https://bugs.webkit.org/show_bug.cgi?id=146064

assert.isNotUndef(WASM.sections);
assert.isNotUndef(WASM.sectionEncodingType);
for (const section of WASM.sections)
    assert.eq(WASM.sectionEncodingType, WASM.description.section[section].type);
