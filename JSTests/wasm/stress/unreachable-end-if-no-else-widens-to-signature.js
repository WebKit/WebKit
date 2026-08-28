// rdar://180535979
// The unreachable End handler synthesizes an else for `if`-without-`else` and
// installs the saved if-param stack as the (reachable) else-arm result. Those
// values must be widened to the block's declared result types before they
// propagate to the parent stack: an unreachable then-arm may have `br 0`'d a
// value that only inhabits the wider result type. Without widening, BBQ's
// emitRefTestOrCast trusts the stale narrow type and elides the IsCell /
// IsWasmGCObject runtime checks.

import * as assert from "../assert.js";

function uleb128(n) { const r = []; do { let b = n & 0x7f; n >>>= 7; if (n) b |= 0x80; r.push(b); } while (n); return r; }
function encodeString(s) { const b = []; for (let i = 0; i < s.length; i++) b.push(s.charCodeAt(i)); return [...uleb128(b.length), ...b]; }
function section(id, content) { return [id, ...uleb128(content.length), ...content]; }

function buildModule() {
    const typeSection = section(1, [
        4,
        0x5F, 0x01, 0x7E, 0x01,                         // type 0: struct { i64 mut }
        0x60, 0x01, 0x64, 0x00, 0x01, 0x6E,             // type 1: func (param (ref 0)) (result anyref)
        0x60, 0x03, 0x7F, 0x6F, 0x64, 0x00, 0x01, 0x7E, // type 2: func (i32, externref, (ref 0)) -> i64
        0x60, 0x00, 0x01, 0x64, 0x00,                   // type 3: func () -> (ref 0)
    ]);
    const funcSection = section(3, [0x02, 0x02, 0x03]);
    const exportSection = section(7, [0x02,
        ...encodeString("test"), 0x00, 0x00,
        ...encodeString("make"), 0x00, 0x01]);

    // (func $test (param $cond i32) (param $ext externref) (param $s (ref 0)) (result i64)
    //   local.get $s
    //   local.get $cond
    //   if (param (ref 0)) (result anyref)
    //     drop
    //     local.get $ext
    //     any.convert_extern
    //     br 0                  ;; then-arm goes unreachable
    //   end                     ;; <- parseUnreachableExpression()::End, synthetic else
    //   ref.cast (ref 0)        ;; must NOT elide IsCell / IsWasmGCObject checks
    //   struct.get 0 0)
    const body0 = [
        0x00,
        0x20, 0x02,
        0x20, 0x00,
        0x04, 0x01,
        0x1A,
        0x20, 0x01,
        0xFB, 0x1A,
        0x0C, 0x00,
        0x0B,
        0xFB, 0x16, 0x00,
        0xFB, 0x02, 0x00, 0x00,
        0x0B,
    ];
    // (func $make (result (ref 0)) i64.const 0x1234 struct.new 0)
    const body1 = [0x00, 0x42, 0xB4, 0x24, 0xFB, 0x00, 0x00, 0x0B];
    const codeSection = section(10, [0x02,
        ...uleb128(body0.length), ...body0,
        ...uleb128(body1.length), ...body1]);
    return new Uint8Array([0x00, 0x61, 0x73, 0x6D, 0x01, 0x00, 0x00, 0x00,
        ...typeSection, ...funcSection, ...exportSection, ...codeSection]);
}

const bytes = buildModule();
assert.truthy(WebAssembly.validate(bytes));
const instance = new WebAssembly.Instance(new WebAssembly.Module(bytes));
const struct = instance.exports.make();

for (let i = 0; i < wasmTestLoopCount; ++i) {
    // cond == 0: synthetic else delivers the (ref 0) param; ref.cast succeeds.
    assert.eq(instance.exports.test(0, null, struct), 0x1234n);
    // cond == 1: then-arm br's an anyref-wrapped JS number to the if's
    // continuation. The post-end value is statically anyref, so ref.cast must
    // perform the full runtime check and trap.
    assert.throws(() => instance.exports.test(1, 1.5, struct), WebAssembly.RuntimeError, "ref.cast failed to cast reference to target heap type");
}
