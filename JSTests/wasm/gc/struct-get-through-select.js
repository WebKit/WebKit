import * as assert from "../assert.js";
import { instantiate } from "./wast-wrapper.js";

let m = instantiate(`
  (module
    (type $vec (struct (field (mut f32)) (field (mut f32))))
    (type $box (struct (field (mut (ref null $vec))) (field (mut i8))))
    (type $doubles (array (mut f64)))
    (type $bytes (array (mut i8)))
    (type $point (struct (field f64) (field (mut f64))))

    (import "env" "mutateBox" (func $mutateBox (param (ref null $box))))
    (import "env" "mutateDoubles" (func $mutateDoubles (param (ref null $doubles))))
    (import "env" "mutatePoint" (func $mutatePoint (param (ref null $point))))

    (func (export "makeVec") (param f32 f32) (result (ref $vec))
      (struct.new $vec (local.get 0) (local.get 1)))
    (func (export "makeBox") (param (ref null $vec) i32) (result (ref $box))
      (struct.new $box (local.get 0) (local.get 1)))

    ;; Both arms' fields are read before the select.
    (func (export "minX") (param $a (ref null $box)) (param $b (ref null $box)) (result f32)
      (local $ax f32) (local $bx f32)
      (local.set $ax (struct.get $vec 0 (struct.get $box 0 (local.get $a))))
      (local.set $bx (struct.get $vec 0 (struct.get $box 0 (local.get $b))))
      (f32.add
        (struct.get $vec 0 (struct.get $box 0
          (select (result (ref null $box)) (local.get $a) (local.get $b) (f32.lt (local.get $ax) (local.get $bx)))))
        (f32.const 0)))

    ;; A store between the reads and the select must be observed.
    (func (export "storeBetween") (param $a (ref null $box)) (param $b (ref null $box)) (param $c i32) (result f32)
      (local $ax f32) (local $bx f32)
      (local.set $ax (struct.get $vec 0 (struct.get $box 0 (local.get $a))))
      (local.set $bx (struct.get $vec 0 (struct.get $box 0 (local.get $b))))
      (struct.set $vec 0 (struct.get $box 0 (local.get $a)) (f32.add (local.get $ax) (local.get $bx)))
      (struct.get $vec 0 (struct.get $box 0
        (select (result (ref null $box)) (local.get $a) (local.get $b) (local.get $c)))))

    ;; Only one arm was read before, so the other may be null and must trap.
    (func (export "oneArmRead") (param $a (ref null $box)) (param $b (ref null $box)) (param $c i32) (result f32)
      (drop (struct.get $vec 0 (struct.get $box 0 (local.get $a))))
      (struct.get $vec 0 (struct.get $box 0
        (select (result (ref null $box)) (local.get $a) (local.get $b) (local.get $c)))))

    ;; A packed field forwarded from a store keeps its narrowing.
    (func (export "packed") (param $a (ref null $box)) (param $b (ref null $box)) (param $c i32) (result i32)
      (struct.set $box 1 (local.get $a) (i32.const 300))
      (drop (struct.get_u $box 1 (local.get $b)))
      (struct.get_u $box 1
        (select (result (ref null $box)) (local.get $a) (local.get $b) (local.get $c))))

    ;; Two reads through the same select see the same value.
    (func (export "repeated") (param $a (ref null $vec)) (param $b (ref null $vec)) (param $c i32) (result f32)
      (drop (struct.get $vec 1 (local.get $a)))
      (drop (struct.get $vec 1 (local.get $b)))
      (f32.add
        (struct.get $vec 1 (select (result (ref null $vec)) (local.get $a) (local.get $b) (local.get $c)))
        (struct.get $vec 1 (select (result (ref null $vec)) (local.get $a) (local.get $b) (local.get $c)))))

    (func (export "makeDoubles") (param f64 i32) (result (ref $doubles))
      (array.new $doubles (local.get 0) (local.get 1)))
    (func (export "makeBytes") (param i32 i32) (result (ref $bytes))
      (array.new $bytes (local.get 0) (local.get 1)))

    (func (export "arrayMax") (param $a (ref null $doubles)) (param $b (ref null $doubles)) (param $i i32) (result f64)
      (local $av f64) (local $bv f64)
      (local.set $av (array.get $doubles (local.get $a) (local.get $i)))
      (local.set $bv (array.get $doubles (local.get $b) (local.get $i)))
      (array.get $doubles
        (select (result (ref null $doubles)) (local.get $a) (local.get $b) (f64.gt (local.get $av) (local.get $bv)))
        (local.get $i)))

    ;; Array reads are used rather than dropped: a dropped array.get is dead and disappears early.
    (func (export "arrayStoreBetween") (param $a (ref null $doubles)) (param $b (ref null $doubles)) (param $i i32) (param $c i32) (result f64)
      (local $sum f64)
      (local.set $sum (f64.add
        (array.get $doubles (local.get $a) (local.get $i))
        (array.get $doubles (local.get $b) (local.get $i))))
      (array.set $doubles (local.get $b) (local.get $i) (f64.const 42))
      (f64.add (local.get $sum)
        (array.get $doubles
          (select (result (ref null $doubles)) (local.get $a) (local.get $b) (local.get $c))
          (local.get $i))))

    ;; Only one arm was read at this index, so the other may be null or too short and must trap.
    (func (export "arrayOneArmRead") (param $a (ref null $doubles)) (param $b (ref null $doubles)) (param $i i32) (param $c i32) (result f64)
      (f64.add
        (array.get $doubles (local.get $a) (local.get $i))
        (array.get $doubles
          (select (result (ref null $doubles)) (local.get $a) (local.get $b) (local.get $c))
          (local.get $i))))

    (func (export "arrayPacked") (param $a (ref null $bytes)) (param $b (ref null $bytes)) (param $i i32) (param $c i32) (result i32)
      (array.set $bytes (local.get $a) (local.get $i) (i32.const 300))
      (i32.add
        (array.get_u $bytes (local.get $b) (local.get $i))
        (array.get_u $bytes
          (select (result (ref null $bytes)) (local.get $a) (local.get $b) (local.get $c))
          (local.get $i))))

    (func (export "arrayLength") (param $a (ref null $doubles)) (param $b (ref null $doubles)) (param $c i32) (result i32)
      (i32.add
        (i32.add (array.len (local.get $a)) (array.len (local.get $b)))
        (array.len (select (result (ref null $doubles)) (local.get $a) (local.get $b) (local.get $c)))))

    (func (export "arrayLengthOneArm") (param $a (ref null $doubles)) (param $b (ref null $doubles)) (param $c i32) (result i32)
      (i32.add
        (array.len (local.get $a))
        (array.len (select (result (ref null $doubles)) (local.get $a) (local.get $b) (local.get $c)))))

    (func (export "setByte") (param (ref null $box)) (param i32)
      (struct.set $box 1 (local.get 0) (local.get 1)))
    (func (export "setDouble") (param (ref null $doubles)) (param i32) (param f64)
      (array.set $doubles (local.get 0) (local.get 1) (local.get 2)))
    (func (export "makePoint") (param f64 f64) (result (ref $point))
      (struct.new $point (local.get 0) (local.get 1)))
    (func (export "setPointY") (param (ref null $point)) (param f64)
      (struct.set $point 1 (local.get 0) (local.get 1)))

    ;; Each arm is read on only one side of a branch, so neither read dominates the select.
    (func (export "nonDominating") (param $a (ref null $vec)) (param $b (ref null $vec)) (param $which i32) (param $c i32) (result f32)
      (local $x f32)
      (if (local.get $which)
        (then (local.set $x (struct.get $vec 0 (local.get $a))))
        (else (local.set $x (struct.get $vec 0 (local.get $b)))))
      (f32.add (local.get $x)
        (struct.get $vec 0 (select (result (ref null $vec)) (local.get $a) (local.get $b) (local.get $c)))))

    (func (export "selectInLoop") (param $a (ref null $vec)) (param $b (ref null $vec)) (param $c i32) (param $n i32) (result f32)
      (local $sum f32)
      (local.set $sum (f32.add (struct.get $vec 0 (local.get $a)) (struct.get $vec 0 (local.get $b))))
      (loop $loop
        (local.set $sum (f32.add (local.get $sum)
          (struct.get $vec 0 (select (result (ref null $vec)) (local.get $a) (local.get $b) (local.get $c)))))
        (struct.set $vec 0 (local.get $a) (f32.add (struct.get $vec 0 (local.get $a)) (f32.const 1)))
        (br_if $loop (local.tee $n (i32.sub (local.get $n) (i32.const 1)))))
      (local.get $sum))

    (func (export "callBetween") (param $a (ref null $box)) (param $b (ref null $box)) (param $c i32) (result i32)
      (local $sum i32)
      (local.set $sum (i32.add (struct.get_u $box 1 (local.get $a)) (struct.get_u $box 1 (local.get $b))))
      (call $mutateBox (local.get $a))
      (i32.add (local.get $sum)
        (struct.get_u $box 1 (select (result (ref null $box)) (local.get $a) (local.get $b) (local.get $c)))))

    (func (export "arrayCallBetween") (param $a (ref null $doubles)) (param $b (ref null $doubles)) (param $i i32) (param $c i32) (result f64)
      (local $sum f64)
      (local.set $sum (f64.add
        (array.get $doubles (local.get $a) (local.get $i))
        (array.get $doubles (local.get $b) (local.get $i))))
      (call $mutateDoubles (local.get $b))
      (f64.add (local.get $sum)
        (array.get $doubles
          (select (result (ref null $doubles)) (local.get $a) (local.get $b) (local.get $c))
          (local.get $i))))

    (func (export "packedSigned") (param $a (ref null $box)) (param $b (ref null $box)) (param $c i32) (result i32)
      (struct.set $box 1 (local.get $a) (i32.const 0x1f0))
      (i32.add
        (struct.get_s $box 1 (local.get $b))
        (struct.get_s $box 1 (select (result (ref null $box)) (local.get $a) (local.get $b) (local.get $c)))))

    (func (export "arrayPackedSigned") (param $a (ref null $bytes)) (param $b (ref null $bytes)) (param $i i32) (param $c i32) (result i32)
      (array.set $bytes (local.get $a) (local.get $i) (i32.const 0x1f0))
      (i32.add
        (array.get_s $bytes (local.get $b) (local.get $i))
        (array.get_s $bytes
          (select (result (ref null $bytes)) (local.get $a) (local.get $b) (local.get $c))
          (local.get $i))))

    ;; An immutable field stays available across writes and calls.
    (func (export "immutableAcrossWrites") (param $a (ref null $point)) (param $b (ref null $point)) (param $c i32) (result f64)
      (local $sum f64)
      (local.set $sum (f64.add (struct.get $point 0 (local.get $a)) (struct.get $point 0 (local.get $b))))
      (struct.set $point 1 (local.get $a) (local.get $sum))
      (call $mutatePoint (local.get $b))
      (f64.add (local.get $sum)
        (struct.get $point 0 (select (result (ref null $point)) (local.get $a) (local.get $b) (local.get $c)))))

    ;; Storing the selected value back through the same select leaves both objects unchanged.
    (func (export "storeSelectedBack") (param $a (ref null $vec)) (param $b (ref null $vec)) (param $c i32) (result f32)
      (local $s (ref null $vec))
      (local $sum f32)
      (local.set $sum (f32.add (struct.get $vec 0 (local.get $a)) (struct.get $vec 0 (local.get $b))))
      (local.set $s (select (result (ref null $vec)) (local.get $a) (local.get $b) (local.get $c)))
      (struct.set $vec 0 (local.get $s) (struct.get $vec 0 (local.get $s)))
      (f32.add (local.get $sum)
        (f32.add (struct.get $vec 0 (local.get $a)) (struct.get $vec 0 (local.get $b)))))
  )
`, {
  env: {
    mutateBox: box => e.setByte(box, 77),
    mutateDoubles: array => e.setDouble(array, 0, 100),
    mutatePoint: point => e.setPointY(point, 5),
  },
});

let e = m.exports;
for (let i = 0; i < wasmTestLoopCount; ++i) {
  let a = e.makeBox(e.makeVec(1, 2), 7);
  let b = e.makeBox(e.makeVec(i & 1 ? 0.5 : 3, 4), 9);
  assert.eq(e.minX(a, b), i & 1 ? 0.5 : 1);

  let c = i & 1;
  let expected = c ? (1 + (i & 1 ? 0.5 : 3)) : (i & 1 ? 0.5 : 3);
  assert.eq(e.storeBetween(a, b, c), expected);

  let a2 = e.makeBox(e.makeVec(5, 6), 1);
  assert.eq(e.oneArmRead(a2, a, 1), 5);

  let p = e.makeBox(null, 1);
  let q = e.makeBox(null, 200);
  assert.eq(e.packed(p, q, 1), 300 & 0xff);
  assert.eq(e.packed(p, q, 0), 200);

  let v1 = e.makeVec(1, 2);
  let v2 = e.makeVec(3, 4);
  assert.eq(e.repeated(v1, v2, c), c ? 4 : 8);

  let aValue = i & 1 ? 5 : 1;
  let da = e.makeDoubles(aValue, 4);
  let db = e.makeDoubles(3, 4);
  let index = i & 3;
  assert.eq(e.arrayMax(da, db, index), i & 1 ? 5 : 3);
  assert.eq(e.arrayStoreBetween(da, db, index, c), aValue + 3 + (c ? aValue : 42));

  let shortArray = e.makeDoubles(7, 1);
  assert.eq(e.arrayOneArmRead(da, shortArray, 0, 0), aValue + 7);

  let ba = e.makeBytes(1, 4);
  let bb = e.makeBytes(200, 4);
  assert.eq(e.arrayPacked(ba, bb, index, 1), 200 + (300 & 0xff));
  assert.eq(e.arrayPacked(ba, bb, index, 0), 200 + 200);

  assert.eq(e.arrayLength(da, shortArray, c), 4 + 1 + (c ? 4 : 1));
  assert.eq(e.arrayLengthOneArm(da, shortArray, 0), 4 + 1);

  let va = e.makeVec(1, 2);
  let vb = e.makeVec(3, 4);
  assert.eq(e.nonDominating(va, vb, 1, 0), 4);
  assert.eq(e.nonDominating(va, vb, 0, 1), 4);

  assert.eq(e.selectInLoop(e.makeVec(1, 2), e.makeVec(3, 4), c, 3), c ? 10 : 13);

  assert.eq(e.callBetween(e.makeBox(null, 7), e.makeBox(null, 9), c), c ? 7 + 9 + 77 : 7 + 9 + 9);
  assert.eq(e.arrayCallBetween(e.makeDoubles(aValue, 4), e.makeDoubles(3, 4), 0, c), aValue + 3 + (c ? aValue : 100));

  assert.eq(e.packedSigned(e.makeBox(null, 1), e.makeBox(null, 200), c), c ? -56 - 16 : -56 - 56);
  assert.eq(e.arrayPackedSigned(e.makeBytes(1, 4), e.makeBytes(200, 4), index, c), c ? -56 - 16 : -56 - 56);

  assert.eq(e.immutableAcrossWrites(e.makePoint(1, 0), e.makePoint(2, 0), c), c ? 4 : 5);

  assert.eq(e.storeSelectedBack(va, vb, c), 8);

  // Each trap is a signal round trip, so they are checked on a sample of iterations.
  if (i % 64)
    continue;
  assert.throws(() => e.oneArmRead(a2, null, 0), WebAssembly.RuntimeError, "null");
  assert.throws(() => e.arrayOneArmRead(da, null, index, 0), WebAssembly.RuntimeError, "null");
  assert.throws(() => e.arrayOneArmRead(da, shortArray, 2, 0), WebAssembly.RuntimeError, "Out of bounds");
  assert.throws(() => e.arrayLengthOneArm(da, null, 0), WebAssembly.RuntimeError, "null");
  assert.throws(() => e.nonDominating(va, null, 1, 0), WebAssembly.RuntimeError, "null");
  assert.throws(() => e.nonDominating(null, vb, 0, 1), WebAssembly.RuntimeError, "null");
}
