import { instantiate } from "../wabt-wrapper.js";
import * as assert from "../assert.js";

const watGetGlobalsSameTempReturnCall = `
(module
  (tag $e (param f32))
  (global $f0 (mut f32)(f32.const 2.0))
  (global $i0 (mut i64)(i64.const 1))
  (func (export "test") (local i64)
    try
      local.get 0
      global.get $f0
      return_call $bar
    catch_all
      global.get $i0
      return_call $foo
    end
  )
  (func $foo)
  (func $bar)
)
`;

const watGetGlobalCatchArgSameTempReturnCall = `
(module
  (tag $e (param i64))
  (global $f0 (mut f32)(f32.const 2.0))
  (global $i0 (mut i64)(i64.const 1))
  (func (export "test") (result i64)
    try (result i64)
      global.get $f0
      global.get $f0
      return_call $bar
    catch $e
      global.get $i0
      i64.add
    end
  )
  (func $bar (param f32) (result i64) i64.const 3)
)
`;

const watGetGlobalsSameTempUnreachable = `
(module
  (tag $e (param f32))
  (global $f0 (mut f32)(f32.const 2.0))
  (global $i0 (mut i64)(i64.const 1))
  (func (export "test") (local i64)
    try
      local.get 0
      global.get $f0
      unreachable
    catch_all
      global.get $i0
      return_call $foo
    end
  )
  (func $foo)
  (func $bar)
)
`;

async function runOne(wat, isUnreachable) {
    const instance = await instantiate(wat, {}, { exceptions: true, tail_call: true });
    const {test} = instance.exports;
    if (isUnreachable) {
        assert.throws(() => {
            test();
        }, WebAssembly.RuntimeError, `Unreachable code`);
    } else {
        test();
    }
}

assert.asyncTest(runOne(watGetGlobalsSameTempReturnCall, false));
assert.asyncTest(runOne(watGetGlobalCatchArgSameTempReturnCall, false));
assert.asyncTest(runOne(watGetGlobalsSameTempUnreachable, true));