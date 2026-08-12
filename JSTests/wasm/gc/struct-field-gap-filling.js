import { instantiate } from "./wast-wrapper.js"
import * as assert from "../assert.js"

// Builds a struct whose fields have the given wasm storage types, and returns the
// payload offset of each field plus the total payload size.
function layoutOf(...fieldTypes) {
    const fields = fieldTypes.map(t => `(field (mut ${t}))`).join(" ");
    const args = fieldTypes.map(t => {
        switch (t) {
        case "f32":
        case "f64":
            return `(${t}.const 0)`;
        case "i64":
            return "(i64.const 0)";
        case "externref":
            return "(ref.null extern)";
        default:
            return "(i32.const 0)";
        }
    }).join(" ");
    const m = instantiate(`
      (module
        (type $s (struct ${fields}))
        (func (export "make") (result (ref $s))
          (struct.new $s ${args})))
    `);
    const o = m.exports.make();
    return { offsets: $vm.wasmStructFieldOffsets(o), size: $vm.wasmStructPayloadSize(o) };
}

function assertLayout(fieldTypes, expectedOffsets, expectedSize) {
    const { offsets, size } = layoutOf(...fieldTypes);
    assert.eq(offsets.length, expectedOffsets.length);
    for (let i = 0; i < expectedOffsets.length; ++i) {
        if (offsets[i] !== expectedOffsets[i])
            throw new Error(`(${fieldTypes}) field ${i}: expected offset ${expectedOffsets[i]}, got ${offsets[i]}`);
    }
    if (size !== expectedSize)
        throw new Error(`(${fieldTypes}) expected payload size ${expectedSize}, got ${size}`);
}

// Payload size is rounded up to a multiple of 8, so it only shrinks once the gap
// filling saves a whole 8-byte slot.

// No gap to fill.
assertLayout(["i32", "i32"], [0, 4], 8);
assertLayout(["i64", "i64"], [0, 8], 16);

// i32 at 0, i64 needs 8-alignment, leaving a 4-byte gap at 4 that the trailing i32 takes.
assertLayout(["i32", "i64", "i32"], [0, 8, 4], 16);

// The trailing field must be aligned within the gap, not just fit in it: the gap is
// [1, 4), and the i16 skips a byte to land on offset 2.
assertLayout(["i8", "i32", "i16"], [0, 4, 2], 8);

// The gap that is tracked is the largest one seen so far. Here i8 at 0 opens [1, 8)
// before the i64; i32 takes [4, 8) and the remainder [1, 4) stays tracked for the i16.
assertLayout(["i8", "i64", "i32", "i16"], [0, 8, 4, 2], 16);

// Only one gap is tracked, so the [1, 2) hole between the two i8s is dropped when the
// larger [2, 8) gap before the i64 replaces it. Both i16s then fit in that larger gap.
assertLayout(["i8", "i8", "i64", "i16", "i16"], [0, 1, 8, 2, 4], 16);

// Interleaved i8/i32, the motivating case: each i8 packs into the alignment hole left
// by the preceding i32 instead of opening a new one. Naive placement would need 24
// payload bytes here.
assertLayout(["i8", "i32", "i8", "i32", "i8", "i32"], [0, 4, 1, 8, 2, 12], 16);

// A field larger than the gap cannot use it.
assertLayout(["i8", "i64", "i64"], [0, 8, 16], 24);

// Reference-typed fields participate too. externref is 8 bytes and 8-aligned.
assertLayout(["i32", "externref", "i32"], [0, 8, 4], 16);

// Gap filling must never reorder fields: a subtype extends its supertype's field list,
// so the shared prefix has to land at the same offsets in both. Otherwise a
// supertype-typed read of a subtype instance reads the wrong bytes.
const sub = instantiate(`
  (module
    (rec
      (type $base (sub (struct (field $a (mut i32)) (field $b (mut i64)))))
      (type $derived (sub $base (struct (field $a (mut i32)) (field $b (mut i64)) (field $c (mut i32))))))
    (func (export "viaBase") (result i64)
      (local $d (ref null $derived))
      (local.set $d (struct.new $derived (i32.const 7) (i64.const 99) (i32.const 5)))
      (struct.get $base $b (local.get $d))))
`);
assert.eq(sub.exports.viaBase(), 99n);

// Values written through a gap-filled offset must round-trip: the fields must not
// overlap each other.
const packed = instantiate(`
  (module
    (type $s (struct (field $a (mut i8)) (field $b (mut i64)) (field $c (mut i32)) (field $d (mut i16))))
    (func (export "sum") (result i64)
      (local $o (ref null $s))
      (local.set $o (struct.new $s (i32.const 0xff) (i64.const 0x1122334455667788) (i32.const 0x0a0b0c0d) (i32.const 0xeeee)))
      (i64.add
        (i64.add
          (i64.extend_i32_u (struct.get_u $s $a (local.get $o)))
          (struct.get $s $b (local.get $o)))
        (i64.add
          (i64.extend_i32_u (struct.get $s $c (local.get $o)))
          (i64.extend_i32_u (struct.get_u $s $d (local.get $o)))))))
`);
assert.eq(packed.exports.sum(), 0xffn + 0x1122334455667788n + 0x0a0b0c0dn + 0xeeeen);
