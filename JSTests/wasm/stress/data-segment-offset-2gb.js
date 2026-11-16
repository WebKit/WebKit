//@ skip if $memoryLimited or $addressBits <= 32
//@ requireOptions("--useExecutableAllocationFuzz=false")
import * as assert from "../assert.js";

function encodeU32(n) {
    const out = [];
    while (true) {
        let b = n & 0x7f;
        n >>>= 7;
        if (n)
            out.push(b | 0x80);
        else {
            out.push(b);
            break;
        }
    }
    return out;
}

function encodeSleb32(v) {
    let n = v | 0;
    const out = [];
    while (true) {
        let b = n & 0x7f;
        n >>= 7;
        if ((n === 0 && (b & 0x40) === 0) || (n === -1 && (b & 0x40) !== 0)) {
            out.push(b);
            break;
        }
        out.push(b | 0x80);
    }
    return out;
}

function section(id, bodyArr) {
    const body = bodyArr;
    const len = encodeU32(body.length);
    return [id, ...len, ...body];
}

// (module
//   (memory 32769 32769)
//   (data (i32.const 0x80000000) "\x2a\x00\x00\x00")
//   (func (export "load") (result i32)
//     i32.const 0x80000000
//     i32.load))
const pages = 32769; // 2GB + 1 page
const cBase = encodeSleb32(0x80000000);

const typeSec = section(1, [1, 0x60, 0, 1, 0x7f]);
const funcSec = section(3, [1, 0]);
const memSec = section(5, [1, 0x01, ...encodeU32(pages), ...encodeU32(pages)]);
const exportSec = section(7, [1, 4, 0x6c, 0x6f, 0x61, 0x64, 0, 0]);
const codeBody = [0, 0x41, ...cBase, 0x28, 0x02, 0x00, 0x0b];
const codeSized = [...encodeU32(codeBody.length), ...codeBody];
const codeSec = section(10, [1, ...codeSized]);
// data: flag 0, i32.const 0x80000000 end, len 4, bytes
const dataPayload = [
    1, // 1 segment
    0, // active mem 0
    0x41, ...cBase, 0x0b,
    4, 0x2a, 0x00, 0x00, 0x00,
];
const dataSec = section(11, dataPayload);

const bytes = new Uint8Array([
    0x00, 0x61, 0x73, 0x6d, 0x01, 0x00, 0x00, 0x00,
    ...typeSec, ...funcSec, ...memSec, ...exportSec, ...codeSec, ...dataSec,
]);

let instance;
try {
    instance = new WebAssembly.Instance(new WebAssembly.Module(bytes));
} catch (e) {
    throw new Error(`instantiation failed (bug 314444 sign-extended data offset?): ${e}`);
}

assert.eq(instance.exports.load(), 42);
