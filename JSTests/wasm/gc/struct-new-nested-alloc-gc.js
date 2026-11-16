import { instantiate } from "./wast-wrapper.js";
// Keep freshly allocated objects reachable through a global while allocating heavily,
// so a GC that lands between the allocation and its field stores would be observable.
const i = instantiate(`
(module
  (type $inner (struct (field $v (mut i32))))
  (type $s (struct (field $a (mut (ref null $inner))) (field $b (mut (ref null $inner)))))
  (global $keep (mut (ref null $s)) (ref.null $s))
  (func (export "run") (param i32) (result i32)
    (local $sum i32)
    (loop $l
      (global.set $keep
        (struct.new $s
          (struct.new $inner (local.get 0))
          (struct.new $inner (i32.add (local.get 0) (i32.const 7)))))
      (local.set $sum (i32.add (local.get $sum)
        (i32.add
          (struct.get $inner $v (ref.as_non_null (struct.get $s $a (ref.as_non_null (global.get $keep)))))
          (struct.get $inner $v (ref.as_non_null (struct.get $s $b (ref.as_non_null (global.get $keep))))))))
      (local.set 0 (i32.sub (local.get 0) (i32.const 1)))
      (br_if $l (local.get 0)))
    (local.get $sum)))
`);
// sum over k=n..1 of (k + k+7)
function want(n){ let s=0; for(let k=n;k>=1;--k) s += k + (k+7); return s|0; }
for (const n of [1, 100, 5000]) {
  const got = i.exports.run(n);
  if (got !== want(n)) throw new Error(`n=${n}: got ${got} want ${want(n)}`);
}
