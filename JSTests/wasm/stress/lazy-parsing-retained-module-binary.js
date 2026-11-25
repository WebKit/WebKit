import * as assert from '../assert.js';

// Data segments, custom section payloads and extended constant expressions point into the
// module binary the module retains, rather than each owning a copy. A module handed over in
// one piece has a binary to point into; one arriving in pieces does not, and has to keep its
// own copy of each. These check both, including the pieces a module can be cut into that
// leave a section reassembled in a buffer of its own.

// (module
//   (memory (export "memory") 1)
//   (global (export "g") i32 (i32.const 1) (i32.const 2) i32.add)   ;; extended constant expr
//   (data (i32.const 0) "\de\ad\be\ef")
//   (func (export "load") (result i32) i32.const 0 i32.load)
// )
// followed by a custom section "meta" with a 3-byte payload.
const bytes = new Uint8Array([
    0x00, 0x61, 0x73, 0x6d, 0x01, 0x00, 0x00, 0x00,
    0x01, 0x05, 0x01, 0x60, 0x00, 0x01, 0x7f,                   // type: () -> i32
    0x03, 0x02, 0x01, 0x00,                                     // one function of type 0
    0x05, 0x03, 0x01, 0x00, 0x01,                               // memory: 1 page
    0x06, 0x09, 0x01, 0x7f, 0x00, 0x41, 0x01, 0x41, 0x02, 0x6a, 0x0b, // global: 1 + 2
    0x07, 0x15, 0x03,                                           // exports
    0x06, 0x6d, 0x65, 0x6d, 0x6f, 0x72, 0x79, 0x02, 0x00,       //   "memory"
    0x01, 0x67, 0x03, 0x00,                                     //   "g"
    0x04, 0x6c, 0x6f, 0x61, 0x64, 0x00, 0x00,                   //   "load"
    0x0a, 0x09, 0x01, 0x07, 0x00, 0x41, 0x00, 0x28, 0x02, 0x00, 0x0b, // func: i32.load at 0
    0x0b, 0x0a, 0x01, 0x00, 0x41, 0x00, 0x0b, 0x04, 0xde, 0xad, 0xbe, 0xef, // data
    0x00, 0x08, 0x04, 0x6d, 0x65, 0x74, 0x61, 0x01, 0x02, 0x03, // custom "meta"
]);

function check(module) {
    const instance = new WebAssembly.Instance(module);
    assert.eq(instance.exports.load(), 0xefbeadde | 0);
    assert.eq(instance.exports.g.value, 3);

    const sections = WebAssembly.Module.customSections(module, "meta");
    assert.eq(sections.length, 1);
    assert.eq(Array.from(new Uint8Array(sections[0])).join(","), "1,2,3");
}

check(new WebAssembly.Module(bytes));

// The same module reassembled from one-byte chunks: no section is contiguous in any buffer
// that outlives the parse, so everything read out of one has to have been copied.
async function streamedOneByteAtATime() {
    const module = await $vm.createWasmStreamingCompilerForCompile(function (compiler) {
        for (const byte of bytes)
            compiler.addBytes(new Uint8Array([byte]));
    });
    check(module);
}

// A trailing custom section small enough that its length is the last thing the parser can
// read ahead over. That leaves the payload behind in the parser's own buffer rather than in
// the retained binary, even though the module was handed over in one piece.
{
    const trailing = new Uint8Array([
        0x00, 0x61, 0x73, 0x6d, 0x01, 0x00, 0x00, 0x00,
        0x01, 0x04, 0x01, 0x60, 0x00, 0x00,
        0x00, 0x03, 0x01, 0x78, 0xaa,   // custom "x", payload 0xaa
    ]);
    const module = new WebAssembly.Module(trailing);
    const sections = WebAssembly.Module.customSections(module, "x");
    assert.eq(sections.length, 1);
    assert.eq(new Uint8Array(sections[0])[0], 0xaa);
}

await assert.asyncTest(streamedOneByteAtATime());
