import * as assert from "../assert.js";
import { compile } from "./wast-wrapper.js";

// A constant expression that the parser cannot fold to a single value is validated once and then
// evaluated again for each instance. Only multi-instruction expressions take that path - a lone
// `i32.const` or `ref.func` is folded by the section parser - so every operation under test is a
// field of one big `struct.new` to make sure it really is evaluated the slow way.

const wat = `(module
  (type $point (struct (field $x i32) (field $y i64)))
  (type $empty (struct (field $a i32) (field $b f64)))
  (type $bytes (array i8))
  (type $floats (array (mut f64)))
  (type $sig (func (result i32)))
  (type $bag (struct
    (field $i32Const i32)
    (field $i64Const i64)
    (field $f32Const f32)
    (field $f64Const f64)
    (field $nullRef (ref null $point))
    (field $funcRef (ref func))
    (field $fromGlobal i32)
    (field $i31 (ref i31))
    (field $structDefault (ref $empty))
    (field $arrayNew (ref $bytes))
    (field $arrayDefault (ref $floats))
    (field $arrayFixed (ref $floats))
    (field $external externref)
    (field $internalized (ref null any))
    (field $i32Arith i32)
    (field $i64Arith i64)))

  (global $imported (import "m" "g") i32)

  (func $callee (type $sig) (i32.const 7))

  (global $bag (ref $bag) (struct.new $bag
    (i32.const 11)
    (i64.const 22)
    (f32.const 1.5)
    (f64.const 2.25)
    (ref.null $point)
    (ref.func $callee)
    (global.get $imported)
    (ref.i31 (i32.const 23))
    (struct.new_default $empty)
    (array.new $bytes (i32.const 3) (i32.const 9))
    (array.new_default $floats (i32.const 2))
    (array.new_fixed $floats 3 (f64.const 1) (f64.const 2) (f64.const 4))
    (extern.convert_any (ref.i31 (i32.const 5)))
    (any.convert_extern (extern.convert_any (ref.i31 (i32.const 6))))
    (i32.mul (i32.add (i32.const 5) (i32.const 3)) (i32.sub (i32.const 4) (i32.const 1)))
    (i64.mul (i64.add (i64.const 5) (i64.const 3)) (i64.sub (i64.const 4) (i64.const 1)))))

  (func (export "i32Const") (result i32) (struct.get $bag $i32Const (global.get $bag)))
  (func (export "i64Const") (result i64) (struct.get $bag $i64Const (global.get $bag)))
  (func (export "f32Const") (result f32) (struct.get $bag $f32Const (global.get $bag)))
  (func (export "f64Const") (result f64) (struct.get $bag $f64Const (global.get $bag)))
  (func (export "nullRefIsNull") (result i32) (ref.is_null (struct.get $bag $nullRef (global.get $bag))))
  (func (export "callFuncRef") (result i32)
    (call_ref $sig (ref.cast (ref $sig) (struct.get $bag $funcRef (global.get $bag)))))
  (func (export "fromGlobal") (result i32) (struct.get $bag $fromGlobal (global.get $bag)))
  (func (export "i31") (result i32) (i31.get_s (struct.get $bag $i31 (global.get $bag))))
  (func (export "structDefaultA") (result i32)
    (struct.get $empty 0 (struct.get $bag $structDefault (global.get $bag))))
  (func (export "structDefaultB") (result f64)
    (struct.get $empty 1 (struct.get $bag $structDefault (global.get $bag))))
  (func (export "arrayNewLen") (result i32) (array.len (struct.get $bag $arrayNew (global.get $bag))))
  (func (export "arrayNewAt") (param i32) (result i32)
    (array.get_u $bytes (struct.get $bag $arrayNew (global.get $bag)) (local.get 0)))
  (func (export "arrayDefaultAt") (param i32) (result f64)
    (array.get $floats (struct.get $bag $arrayDefault (global.get $bag)) (local.get 0)))
  (func (export "arrayFixedLen") (result i32) (array.len (struct.get $bag $arrayFixed (global.get $bag))))
  (func (export "arrayFixedAt") (param i32) (result f64)
    (array.get $floats (struct.get $bag $arrayFixed (global.get $bag)) (local.get 0)))
  (func (export "external") (result externref) (struct.get $bag $external (global.get $bag)))
  (func (export "internalized") (result i32)
    (i31.get_s (ref.cast (ref i31) (struct.get $bag $internalized (global.get $bag)))))
  (func (export "i32Arith") (result i32) (struct.get $bag $i32Arith (global.get $bag)))
  (func (export "i64Arith") (result i64) (struct.get $bag $i64Arith (global.get $bag)))
  (func (export "bag") (result (ref $bag)) (global.get $bag))
)`;

const module = compile(wat);

function check(instance, importedValue) {
    const e = instance.exports;
    assert.eq(e.i32Const(), 11);
    assert.eq(e.i64Const(), 22n);
    assert.eq(e.f32Const(), 1.5);
    assert.eq(e.f64Const(), 2.25);
    assert.eq(e.nullRefIsNull(), 1);
    assert.eq(e.callFuncRef(), 7);
    assert.eq(e.fromGlobal(), importedValue);
    assert.eq(e.i31(), 23);
    assert.eq(e.structDefaultA(), 0);
    assert.eq(e.structDefaultB(), 0);
    assert.eq(e.arrayNewLen(), 9);
    assert.eq(e.arrayNewAt(0), 3);
    assert.eq(e.arrayNewAt(8), 3);
    assert.eq(e.arrayDefaultAt(0), 0);
    assert.eq(e.arrayDefaultAt(1), 0);
    assert.eq(e.arrayFixedLen(), 3);
    assert.eq(e.arrayFixedAt(0), 1);
    assert.eq(e.arrayFixedAt(1), 2);
    assert.eq(e.arrayFixedAt(2), 4);
    assert.eq(e.external(), 5);
    assert.eq(e.internalized(), 6);
    assert.eq(e.i32Arith(), 24);
    assert.eq(e.i64Arith(), 24n);
}

const first = new WebAssembly.Instance(module, { m: { g: 1 } });
check(first, 1);

// Evaluating the same expression again has to produce the same values, and fresh objects.
const second = new WebAssembly.Instance(module, { m: { g: 2 } });
check(second, 2);
check(first, 1);
assert.falsy(first.exports.bag() === second.exports.bag());
