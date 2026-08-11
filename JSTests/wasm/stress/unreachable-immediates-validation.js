import * as assert from "../assert.js";

// Immediates in unreachable code are still validated: the spec's validation rules do not depend on
// reachability. These modules are assembled by hand because a text assembler rejects them outright.

const SECTION_TYPE = 1;
const SECTION_FUNCTION = 3;
const SECTION_TABLE = 4;
const SECTION_MEMORY = 5;
const SECTION_CODE = 10;

function leb(value) {
    let bytes = [];
    do {
        let byte = value & 0x7f;
        value >>>= 7;
        if (value)
            byte |= 0x80;
        bytes.push(byte);
    } while (value);
    return bytes;
}

function section(id, payload) {
    return [id, ...leb(payload.length), ...payload];
}

function moduleBytes({ withMemory = false, withTable = false, withExternrefTable = false, withStructType = false, body }) {
    let bytes = [0x00, 0x61, 0x73, 0x6d, 0x01, 0x00, 0x00, 0x00];
    if (withStructType)
        bytes.push(...section(SECTION_TYPE, [0x02, 0x60, 0x00, 0x00, 0x5f, 0x00])); // type 0: () -> (), type 1: (struct)
    else
        bytes.push(...section(SECTION_TYPE, [0x01, 0x60, 0x00, 0x00])); // one type: () -> ()
    bytes.push(...section(SECTION_FUNCTION, [0x01, 0x00]));
    if (withTable)
        bytes.push(...section(SECTION_TABLE, [0x01, 0x70, 0x00, 0x01])); // one funcref table, min 1
    else if (withExternrefTable)
        bytes.push(...section(SECTION_TABLE, [0x01, 0x6f, 0x00, 0x01])); // one externref table, min 1
    if (withMemory)
        bytes.push(...section(SECTION_MEMORY, [0x01, 0x00, 0x01])); // one memory, min 1
    const code = [0x00, 0x00, ...body, 0x0b]; // no locals, unreachable, body, end
    bytes.push(...section(SECTION_CODE, [0x01, ...leb(code.length), ...code]));
    return new Uint8Array(bytes);
}

function assertInvalid(description, options) {
    const bytes = moduleBytes(options);
    try {
        new WebAssembly.Module(bytes);
    } catch (error) {
        assert.truthy(error instanceof WebAssembly.CompileError, `${description}: expected CompileError, got ${error}`);
        return;
    }
    throw new Error(`${description}: module was accepted but is invalid`);
}

function assertValid(description, options) {
    new WebAssembly.Module(moduleBytes(options));
}

// A memarg alignment above the access's natural alignment is invalid.
assertInvalid("i32.load align=8", { withMemory: true, body: [0x28, 0x03, 0x00] });
assertInvalid("i32.load align=2^63", { withMemory: true, body: [0x28, 0x3f, 0x00] });
assertInvalid("i32.store align=8", { withMemory: true, body: [0x36, 0x03, 0x00] });
assertInvalid("i32.load8_s align=2", { withMemory: true, body: [0x2c, 0x01, 0x00] });
assertInvalid("i64.load align=16", { withMemory: true, body: [0x29, 0x04, 0x00] });
assertValid("i32.load align=4", { withMemory: true, body: [0x28, 0x02, 0x00] });
assertValid("i32.load align=1", { withMemory: true, body: [0x28, 0x00, 0x00] });

// A table index must exist. Only index 0 does here.
assertInvalid("table.get 5", { withTable: true, body: [0x25, 0x05] });
assertInvalid("table.set 5", { withTable: true, body: [0x26, 0x05] });
assertInvalid("table.get 0, no table section", { body: [0x25, 0x00] });
assertValid("table.get 0", { withTable: true, body: [0x25, 0x00] });

// call_indirect validates both of its immediates, and needs a table at all.
assertInvalid("call_indirect type 0 table 7", { withTable: true, body: [0x11, 0x00, 0x07] });
assertInvalid("call_indirect type 99 table 0", { withTable: true, body: [0x11, 0x63, 0x00] });
assertInvalid("call_indirect with no table section", { body: [0x11, 0x00, 0x00] });
assertInvalid("call_indirect over an externref table", { withExternrefTable: true, body: [0x11, 0x00, 0x00] });
assertInvalid("call_indirect with a struct type index", { withTable: true, withStructType: true, body: [0x11, 0x01, 0x00] });
assertInvalid("return_call_indirect over an externref table", { withExternrefTable: true, body: [0x13, 0x00, 0x00] });
assertInvalid("return_call_indirect with a struct type index", { withTable: true, withStructType: true, body: [0x13, 0x01, 0x00] });
assertValid("call_indirect type 0 table 0", { withTable: true, body: [0x11, 0x00, 0x00] });
assertValid("call_indirect type 0 table 0 alongside a struct type", { withTable: true, withStructType: true, body: [0x11, 0x00, 0x00] });

// ref.func needs an index inside the function index space, and a declaration.
assertInvalid("ref.func 99", { body: [0xd2, 0x63, 0x1a] });
assertInvalid("ref.func 0 undeclared", { body: [0xd2, 0x00, 0x1a] });
