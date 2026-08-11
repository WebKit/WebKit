import * as assert from "../assert.js";
import { compile, instantiate } from "./wast-wrapper.js";

const wat = `
(module

  (func (export "test")
    (result i32)

    (ref.eq
      (call $getI31Ref (i32.const 0x40000000))
      (call $getI31RefConst))
  )

  (func $getI31Ref
    (param $v i32)
    (result i31ref)

    (ref.i31 (local.get $v))
  )

  (func $getI31RefConst
    (result i31ref)

    (ref.i31 (i32.const 0x40000000))
  )
)
`;

let instance = instantiate(wat);

for (let i = 0; i < testLoopCount; i++)
  assert.eq(instance.exports.test(), 1);
