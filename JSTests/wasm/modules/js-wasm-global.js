import { constant } from "./constant.wasm"
import * as assert from '../assert.js';

assert.isNumber(constant);
assert.eq(constant, 42);

const instanceExports = new WebAssembly.Instance(new WebAssembly.Module(new Uint8Array([
    0x00, 0x61, 0x73, 0x6d, 0x01, 0x00, 0x00, 0x00,
    0x06, 0x06, 0x01, 0x7f, 0x00, 0x41, 0x2a, 0x0b,
    0x07, 0x05, 0x01, 0x01, 0x63, 0x03, 0x00,
]))).exports;
assert.instanceof(instanceExports.c, WebAssembly.Global);
assert.eq(instanceExports.c.value, 42);
