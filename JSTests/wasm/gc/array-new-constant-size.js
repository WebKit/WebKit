import { instantiate } from "./wast-wrapper.js";
// array.new_default with a constant length picks its size class at compile time. Cover a small
// array (inline fast path), one straddling the largeCutoff, and one that must be a precise
// allocation (no fast path at all).
function make(elementType, len, init, get) {
  return instantiate(`
(module
  (type $a (array (mut ${elementType})))
  (func (export "t") (param i32) (result ${elementType})
    (local $arr (ref null $a))
    (loop $l
      (local.set $arr (array.new_default $a (i32.const ${len})))
      (array.set $a (ref.as_non_null (local.get $arr)) (i32.const ${len - 1}) (${init}))
      (local.set 0 (i32.sub (local.get 0) (i32.const 1)))
      (br_if $l (local.get 0)))
    (${get} $a (ref.as_non_null (local.get $arr)) (i32.const ${len - 1}))))
`);
}

for (const [elementType, init, get, want] of [
  ["i32", "i32.const 7", "array.get", 7],
  ["i64", "i64.const 7", "array.get", 7n],
  ["f64", "f64.const 7", "array.get", 7],
]) {
  // 1 and 8 stay well inside the size classes; 4096/8192 cross largeCutoff into precise allocation.
  for (const len of [1, 8, 100, 4096, 8192, 65536]) {
    const i = make(elementType, len, init, get);
    for (const n of [1, 5, 2000]) {
      const got = i.exports.t(n);
      if (got !== want)
        throw new Error(`${elementType} len=${len} n=${n}: got ${got} want ${want}`);
    }
  }
}
