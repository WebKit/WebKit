import { instantiate } from "./wast-wrapper.js";
// A load of $a.f cached across an allocation must still see a store made through
// an aliasing reference AFTER the allocation.
const i = instantiate(`
(module
  (type $s (struct (field $f (mut i32))))
  (type $t (struct (field $g (mut i32))))
  (func (export "t") (param i32) (result i32)
    (local $a (ref null $s))
    (local $sum i32)
    (local.set $a (struct.new $s (i32.const 1)))
    (loop $l
      ;; read, allocate (must not clobber the read), store, read again
      (local.set $sum (i32.add (local.get $sum) (struct.get $s $f (ref.as_non_null (local.get $a)))))
      (drop (struct.new $t (i32.const 9)))
      (struct.set $s $f (ref.as_non_null (local.get $a)) (i32.const 2))
      (local.set $sum (i32.add (local.get $sum) (struct.get $s $f (ref.as_non_null (local.get $a)))))
      (struct.set $s $f (ref.as_non_null (local.get $a)) (i32.const 1))
      (local.set 0 (i32.sub (local.get 0) (i32.const 1)))
      (br_if $l (local.get 0)))
    (local.get $sum)))
`);
// each iteration contributes 1 + 2 = 3
for (const n of [1, 10, 1000, 200000]) {
  const got = i.exports.t(n), want = 3 * n;
  if (got !== want) throw new Error(`n=${n}: got ${got} want ${want}`);
}
