import * as assert from '../assert.js';

// A streamed module has no retained binary to point into, so each data segment keeps its own
// copy of its bytes. A zero-length segment copies nothing, which must not be mistaken for
// "the source is still there to point at": the section payload it was read from is released as
// soon as the section has been parsed, and the segment outlives the module's instantiation.

function leb(value) {
    let result = [];
    do {
        let byte = value & 0x7f;
        value >>>= 7;
        if (value)
            byte |= 0x80;
        result.push(byte);
    } while (value);
    return result;
}

function section(id, payload) {
    return [id, ...leb(payload.length), ...payload];
}

// (module
//   (memory (export "mem") 1)
//   (data (i32.const 0) "")           ;; segment 0, active and empty
//   (data "")                         ;; segment 1, passive and empty
//   (data (i32.const 4) "hi")         ;; segment 2, active and not empty
//   (func (export "f") (result i32)
//     (memory.init 1 (i32.const 0) (i32.const 0) (i32.const 0))
//     (i32.load8_u (i32.const 4))))
const body = [
    0x00,                          // no locals
    0x41, 0x00,                    // i32.const 0  (dst)
    0x41, 0x00,                    // i32.const 0  (src)
    0x41, 0x00,                    // i32.const 0  (size)
    0xfc, 0x08, 0x01, 0x00,        // memory.init segment 1, memory 0
    0x41, 0x04,                    // i32.const 4
    0x2d, 0x00, 0x00,              // i32.load8_u align=1 offset=0
    0x0b,                          // end
];

const bytes = new Uint8Array([
    0x00, 0x61, 0x73, 0x6d, 0x01, 0x00, 0x00, 0x00,
    ...section(1, [0x01, 0x60, 0x00, 0x01, 0x7f]),
    ...section(3, [0x01, 0x00]),
    ...section(5, [0x01, 0x00, 0x01]),
    ...section(7, [0x02,
        0x03, 0x6d, 0x65, 0x6d, 0x02, 0x00,
        0x01, 0x66, 0x00, 0x00]),
    ...section(12, [0x03]),
    ...section(10, [0x01, ...leb(body.length), ...body]),
    ...section(11, [0x03,
        0x00, 0x41, 0x00, 0x0b, 0x00,              // active, offset 0, 0 bytes
        0x01, 0x00,                                // passive, 0 bytes
        0x00, 0x41, 0x04, 0x0b, 0x02, 0x68, 0x69]), // active, offset 4, "hi"
]);

function checkInstance(instance) {
    assert.eq(instance.exports.f(), 0x68);
    assert.eq(new Uint8Array(instance.exports.mem.buffer)[5], 0x69);
}

// Handed over in one piece, the binary is retained and the segments point into it.
checkInstance(new WebAssembly.Instance(new WebAssembly.Module(bytes)));

async function testStreaming() {
    for (const chunkSize of [bytes.length, 1]) {
        const { instance } = await $vm.createWasmStreamingCompilerForInstantiate(function (compiler) {
            for (let i = 0; i < bytes.length; i += chunkSize)
                compiler.addBytes(bytes.subarray(i, i + chunkSize));
        }, {});
        checkInstance(instance);
    }
}

await assert.asyncTest(testStreaming());
