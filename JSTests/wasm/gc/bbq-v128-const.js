//@ requireOptions("--useWasmSIMD=1")
//@ skip unless $isSIMDPlatform

import * as assert from "../assert.js";
import { instantiate } from "./wast-wrapper.js";

{
    const m = instantiate(`
      (module
        (type (array v128))
        (func (export "isZero") (param i32) (result i32)
          (i64x2.all_true
            (i64x2.eq
              (array.get 0 (array.new_default 0 (i32.const 4)) (local.get 0))
              (v128.const i64x2 0 0))))
      )
    `);
    for (var i = 0; i < 4; i++)
        assert.eq(m.exports.isZero(i), 1);
}

{
    const m = instantiate(`
      (module
        (type (struct (field v128)))
        (func (export "isZero") (result i32)
          (i64x2.all_true
            (i64x2.eq
              (struct.get 0 0 (struct.new_default 0))
              (v128.const i64x2 0 0))))
      )
    `);
    assert.eq(m.exports.isZero(), 1);
}

{
    const m = instantiate(`
      (module
        (type (struct (field v128 v128)))
        (func (export "isZero") (result i32)
          (local (ref null 0))
          (local.set 0 (struct.new_default 0))
          (i32.and
            (i64x2.all_true
              (i64x2.eq
                (struct.get 0 0 (local.get 0))
                (v128.const i64x2 0 0)))
            (i64x2.all_true
              (i64x2.eq
                (struct.get 0 1 (local.get 0))
                (v128.const i64x2 0 0)))))
      )
    `);
    assert.eq(m.exports.isZero(), 1);
}
