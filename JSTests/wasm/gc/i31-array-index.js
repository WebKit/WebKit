import * as assert from "../assert.js";
import { compile, instantiate } from "./wast-wrapper.js";

const wat = `
(module
  (type $a (array (mut i32)))

  (func (export "testSignedConst")
    (result i32)

    (array.new $a (i32.const 101)(i32.const 2))
    (i31.get_s (ref.i31 (i32.const 1)))
    (array.get $a)
  )

  (func (export "testUnsignedConst")
    (result i32)

    (array.new $a (i32.const 202)(i32.const 2))
    (i31.get_u (ref.i31 (i32.const 1)))
    (array.get $a)
  )

  (func (export "testSigned")
    (result i32)

    (array.new $a (i32.const 303)(i32.const 2))
    (i31.get_s (call $getI31Ref (i32.const 1)))
    (array.get $a)
  )

  (func (export "testUnsigned")
    (result i32)

    (array.new $a (i32.const 404)(i32.const 2))
    (i31.get_u (call $getI31Ref (i32.const 1)))
    (array.get $a)
  )

  (func $getI31Ref
    (param $v i32)
    (result i31ref)

    (ref.i31 (local.get $v))
  )
)`;

let instance = instantiate(wat);

globalThis.testLoopCount ??= 10000;

for (let i = 0; i < testLoopCount; i++) {
  assert.eq(instance.exports.testSignedConst(), 101);
  assert.eq(instance.exports.testUnsignedConst(), 202);
  assert.eq(instance.exports.testSigned(), 303);
  assert.eq(instance.exports.testUnsigned(), 404);

}