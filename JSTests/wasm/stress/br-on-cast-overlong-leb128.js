"use strict";
// Canonical and overlong LEB128 encodings of a GC sub-opcode must produce
// identical execution semantics. This test checks that for br_on_cast with
// FLAGS=0x03 - divergence means IPInt is reading the flags byte from a
// position the encoding moved.

function uleb(n) { const o=[]; do { let b=n&0x7f; n>>>=7; if(n) b|=0x80; o.push(b); } while(n); return o; }
function section(id, b) { return [id, ...uleb(b.length), ...b]; }
function str(s) { return [s.length, ...Array.from(s, c => c.charCodeAt(0))]; }

function buildModule(brOnCast) {
    const typeSec = [
        0x03,
        0x5f, 0x00,                          // type 0: (struct)
        0x60, 0x00, 0x00,                    // type 1: () -> void
        0x60, 0x00, 0x01, 0x7f,              // type 2: () -> i32
    ];
    const funcSec = [0x02, 0x01, 0x02];
    // global 0: (mut (ref null 0)) init = (struct.new_default 0)
    const globalSec = [0x01, 0x63, 0x00, 0x01, 0xfb, 0x01, 0x00, 0x0b];
    const exportSec = [
        0x02,
        ...str("poison"),      0x00, 0x00,
        ...str("is_poisoned"), 0x00, 0x01,
    ];
    const poisonBody = [
        0x00,
        0x02, 0x63, 0x00,                    // block (result (ref null 0))
            0xd0, 0x6e,                      //   ref.null any
            ...brOnCast,                     //   br_on_cast 0  (FLAGS=0x03 in either encoding)
            0x1a,                            //   drop
            0xfb, 0x01, 0x00,                //   struct.new_default 0  (only reached if cast missed)
            0x0c, 0x00,                      //   br 0
        0x0b,
        0x24, 0x00,                          // global.set 0
        0x0b,
    ];
    const checkBody = [
        0x00,
        0x23, 0x00,                          // global.get 0
        0xd1,                                // ref.is_null
        0x0b,
    ];
    const codeSec = [
        0x02,
        ...uleb(poisonBody.length), ...poisonBody,
        ...uleb(checkBody.length),  ...checkBody,
    ];
    return new Uint8Array([
        0x00, 0x61, 0x73, 0x6d, 0x01, 0x00, 0x00, 0x00,
        ...section(1, typeSec),
        ...section(3, funcSec),
        ...section(6, globalSec),
        ...section(7, exportSec),
        ...section(10, codeSec),
    ]);
}

//                       prefix sub-op            flags label ht1   ht2
const CANONICAL = [0xfb, 0x18,            0x03, 0x00, 0x6e, 0x00];
const OVERLONG  = [0xfb, 0x98,0x80,0x00,  0x03, 0x00, 0x6e, 0x00];

const a = new WebAssembly.Instance(new WebAssembly.Module(buildModule(CANONICAL)));
const b = new WebAssembly.Instance(new WebAssembly.Module(buildModule(OVERLONG)));

a.exports.poison();
b.exports.poison();

const rA = a.exports.is_poisoned();
const rB = b.exports.is_poisoned();

if (rA !== rB)
    throw new Error("LEB encoding of GC sub-opcode affects br_on_cast semantics: "
                    + "canonical=" + rA + " overlong=" + rB
                    + " — IPInt reads flags from PC at hardcoded offset");
