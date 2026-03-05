// see rdar://170534591

import * as assert from "../assert.js";

var wasm_code = new Uint8Array([0x00,0x61,0x73,0x6D,0x01,0x00,0x00,0x00,0x01,0x85,0x00,0x01,0x60,0x00,0x01,0x7C,0x03,0x82,0x00,0x01,0x00,0x0A,0x8A,0x00,0x01,0x08,0x00,0x00,0x2C,0x05,0x83,0xFD,0x6D,0x0B,]);

let exception = undefined;

try {
    var wasm_module = new WebAssembly.Module(wasm_code);
    var wasm_instance = new WebAssembly.Instance(wasm_module);
    var f = wasm_instance.exports.main;
    f();
} catch(e) {
    exception = "" + e;
}

assert.eq(exception, "CompileError: WebAssembly.Module doesn't parse at byte 3: load/store instruction without memory, in function at index 0 (evaluating 'new WebAssembly.Module(wasm_code)')");

// this should throw a parse error instead of triggering a runtime assertion
