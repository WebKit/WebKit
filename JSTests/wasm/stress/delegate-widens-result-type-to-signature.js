import * as assert from "../assert.js";

function uleb128(n) { const r = []; do { let b = n & 0x7f; n >>>= 7; if (n) b |= 0x80; r.push(b); } while (n); return r; }
function encodeString(s) { const b = []; for (let i = 0; i < s.length; i++) b.push(s.charCodeAt(i)); return [...uleb128(b.length), ...b]; }
function section(id, content) { return [id, ...uleb128(content.length), ...content]; }

function buildModule() {
    const typeSection = section(1, [
        3,
        0x5F, 0x01, 0x7E, 0x01,                         // type 0: struct { i64 mut }
        0x60, 0x03, 0x7F, 0x6F, 0x64, 0x00, 0x01, 0x7E, // type 1: func (i32, externref, (ref 0)) -> i64
        0x60, 0x00, 0x01, 0x64, 0x00,                   // type 2: func () -> (ref 0)
    ]);
    const funcSection = section(3, [0x02, 0x01, 0x02]);
    const exportSection = section(7, [0x02,
        ...encodeString("test"), 0x00, 0x00,
        ...encodeString("make"), 0x00, 0x01]);

    // (func $test (param $cond i32) (param $ext externref) (param $s (ref 0)) (result i64)
    //   try (result i64)                 ;; outer: delegate target
    //     try (result anyref)            ;; inner
    //       local.get $ext
    //       any.convert_extern           ;; -> anyref (NaN-boxed JS number)
    //       local.get $cond
    //       br_if 0                      ;; carry the anyref to the inner continuation
    //       drop
    //       local.get $s                 ;; fallthrough: (ref 0), a subtype of anyref
    //     delegate 0                     ;; terminates inner try; result MUST widen to anyref
    //     ref.cast (ref 0)               ;; must NOT elide IsCell / IsWasmGCObject checks
    //     struct.get 0 0
    //   catch_all
    //     i64.const 0
    //   end)
    const body0 = [
        0x00,
        0x06, 0x7E,             // try (result i64)
          0x06, 0x6E,           //   try (result anyref)
            0x20, 0x01,         //     local.get 1
            0xFB, 0x1A,         //     any.convert_extern
            0x20, 0x00,         //     local.get 0
            0x0D, 0x00,         //     br_if 0
            0x1A,               //     drop
            0x20, 0x02,         //     local.get 2
          0x18, 0x00,           //   delegate 0
          0xFB, 0x16, 0x00,     //   ref.cast (ref 0)
          0xFB, 0x02, 0x00, 0x00, // struct.get 0 0
        0x19,                   // catch_all
          0x42, 0x00,           //   i64.const 0
        0x0B,                   // end (outer try)
        0x0B,                   // end (func)
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
    // cond == 0: br_if not taken; the try body's (ref 0) fallthrough survives the
    // delegate. Widened to anyref, ref.cast succeeds and struct.get reads the field.
    assert.eq(instance.exports.test(0, null, struct), 0x1234n);
    // cond == 1: br_if delivers an anyref-wrapped JS number to the inner continuation.
    // The post-delegate value is statically anyref, so ref.cast must perform the full
    // runtime check and trap rather than dereference the non-cell value.
    assert.throws(() => instance.exports.test(1, 1.5, struct), WebAssembly.RuntimeError, "ref.cast failed to cast reference to target heap type");
}
