import { instantiate } from "../wabt-wrapper.js";
import * as assert from "../assert.js";

let wat = `
(module
    (func (export "divsConstantLeft32") (param i32) (result i32)
        i32.const 729
        local.get 0
        i32.div_s
    )

    (func (export "divuConstantLeft32") (param i32) (result i32)
        i32.const 729
        local.get 0
        i32.div_u
    )

    (func (export "divsConstantRight32") (param i32) (result i32)
        local.get 0
        i32.const 125
        i32.div_s
    )

    (func (export "divuConstantRight32") (param i32) (result i32)
        local.get 0
        i32.const 125
        i32.div_u
    )

    (func (export "divsConstantLeft64") (param i64) (result i64)
        i64.const 729
        local.get 0
        i64.div_s
    )

    (func (export "divuConstantLeft64") (param i64) (result i64)
        i64.const 729
        local.get 0
        i64.div_u
    )

    (func (export "divsConstantRight64") (param i64) (result i64)
        local.get 0
        i64.const 125
        i64.div_s
    )

    (func (export "divuConstantRight64") (param i64) (result i64)
        local.get 0
        i64.const 125
        i64.div_u
    )

    (func (export "divsByMinusOne32") (param i32) (result i32)
        local.get 0
        i32.const -1
        i32.div_s
    )

    (func (export "divuByMinusOne32") (param i32) (result i32)
        local.get 0
        i32.const -1
        i32.div_u
    )

    (func (export "divsIntMin32") (param i32) (result i32)
        i32.const 0x80000000
        local.get 0
        i32.div_s
    )

    (func (export "divuIntMin32") (param i32) (result i32)
        i32.const 0x80000000
        local.get 0
        i32.div_u
    )

    (func (export "divsByZero32") (param i32) (result i32)
        local.get 0
        i32.const 0
        i32.div_s
    )

    (func (export "divuByZero32") (param i32) (result i32)
        local.get 0
        i32.const 0
        i32.div_u
    )

    (func (export "divsByMinusOne64") (param i64) (result i64)
        local.get 0
        i64.const -1
        i64.div_s
    )

    (func (export "divuByMinusOne64") (param i64) (result i64)
        local.get 0
        i64.const -1
        i64.div_u
    )

    (func (export "divsIntMin64") (param i64) (result i64)
        i64.const 0x8000000000000000
        local.get 0
        i64.div_s
    )

    (func (export "divuIntMin64") (param i64) (result i64)
        i64.const 0x8000000000000000
        local.get 0
        i64.div_u
    )

    (func (export "divsByZero64") (param i64) (result i64)
        local.get 0
        i64.const 0
        i64.div_s
    )

    (func (export "divuByZero64") (param i64) (result i64)
        local.get 0
        i64.const 0
        i64.div_u
    )

    (func (export "foldableDivsByZero32")
        i32.const 42
        i32.const 0
        i32.div_s
        drop
    )

    (func (export "foldableDivuByZero32")
        i32.const 42
        i32.const 0
        i32.div_u
        drop
    )

    (func (export "foldableDivsMinByMinusOne32")
        i32.const 0x80000000
        i32.const -1
        i32.div_s
        drop
    )

    (func (export "foldableDivuByZero64")
        i64.const 42
        i64.const 0
        i64.div_u
        drop
    )

    (func (export "foldableDivsByZero64")
        i64.const 42
        i64.const 0
        i64.div_s
        drop
    )

    (func (export "foldableDivsMinByMinusOne64")
        i64.const 0x8000000000000000
        i64.const -1
        i64.div_s
        drop
    )
)
`;

async function test() {
  const instance = await instantiate(wat, {}, {});
  const {
    divsConstantLeft32, divuConstantLeft32,
    divsConstantRight32, divuConstantRight32,
    divsConstantLeft64, divuConstantLeft64,
    divsConstantRight64, divuConstantRight64,
    divsByMinusOne32, divuByMinusOne32,
    divsIntMin32, divuIntMin32,
    divsByZero32, divuByZero32,
    divsByMinusOne64, divuByMinusOne64,
    divsIntMin64, divuIntMin64,
    divsByZero64, divuByZero64,
    foldableDivsByZero32, foldableDivuByZero32, foldableDivsMinByMinusOne32,
    foldableDivsByZero64, foldableDivuByZero64, foldableDivsMinByMinusOne64
  } = instance.exports;

  // 32-bit integers, one constant operand

  assert.eq(divsConstantLeft32(9), 81);
  assert.eq(divuConstantLeft32(9), 81);
  assert.eq(divsConstantRight32(3125), 25);
  assert.eq(divuConstantRight32(3125), 25);

  assert.eq(divsByMinusOne32(42), -42);
  assert.throws(() => divsByMinusOne32(-0x80000000), WebAssembly.RuntimeError, "Integer overflow");

  assert.eq(divuByMinusOne32(42), 0);
  assert.eq(divuByMinusOne32(-0x80000000), 0); // Not an error for unsigned division.

  assert.eq(divsIntMin32(4), -0x20000000);
  assert.throws(() => divsIntMin32(-1), WebAssembly.RuntimeError, "Integer overflow");
  assert.throws(() => divsIntMin32(0), WebAssembly.RuntimeError, "Division by zero");

  assert.eq(divuIntMin32(1), -0x80000000);
  assert.eq(divuIntMin32(-1), 0); // Not an error for unsigned division.
  assert.throws(() => divuIntMin32(0), WebAssembly.RuntimeError, "Division by zero");

  assert.throws(() => divsByZero32(42), WebAssembly.RuntimeError, "Division by zero");

  assert.throws(() => divuByZero32(42), WebAssembly.RuntimeError, "Division by zero");

  // 64-bit integers, one constant operand

  assert.eq(divsConstantLeft64(9n), 81n);
  assert.eq(divuConstantLeft64(9n), 81n);
  assert.eq(divsConstantRight64(3125n), 25n);
  assert.eq(divuConstantRight64(3125n), 25n);

  assert.eq(divsByMinusOne64(42n), -42n);
  assert.throws(() => divsByMinusOne64(-0x8000000000000000n), WebAssembly.RuntimeError, "Integer overflow");

  assert.eq(divuByMinusOne64(42n), 0n);
  assert.eq(divuByMinusOne64(-0x8000000000000000n), 0n); // Not an error for unsigned division.

  assert.eq(divsIntMin64(4n), -0x2000000000000000n);
  assert.throws(() => divsIntMin64(-1n), WebAssembly.RuntimeError, "Integer overflow");
  assert.throws(() => divsIntMin64(0n), WebAssembly.RuntimeError, "Division by zero");

  assert.eq(divuIntMin64(1n), -0x8000000000000000n);
  assert.eq(divuIntMin64(-1n), 0n); // Not an error for unsigned division.
  assert.throws(() => divuIntMin64(0n), WebAssembly.RuntimeError, "Division by zero");

  assert.throws(() => divsByZero64(42n), WebAssembly.RuntimeError, "Division by zero");

  assert.throws(() => divuByZero64(42n), WebAssembly.RuntimeError, "Division by zero");

  // Two constant operands

  assert.throws(foldableDivsByZero32, WebAssembly.RuntimeError, "Division by zero");
  assert.throws(foldableDivuByZero32, WebAssembly.RuntimeError, "Division by zero");
  assert.throws(foldableDivsMinByMinusOne32, WebAssembly.RuntimeError, "Integer overflow");

  assert.throws(foldableDivsByZero64, WebAssembly.RuntimeError, "Division by zero");
  assert.throws(foldableDivuByZero64, WebAssembly.RuntimeError, "Division by zero");
  assert.throws(foldableDivsMinByMinusOne64, WebAssembly.RuntimeError, "Integer overflow");
}

await assert.asyncTest(test());
