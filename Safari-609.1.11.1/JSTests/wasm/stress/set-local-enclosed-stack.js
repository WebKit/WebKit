import * as assert from '../assert.js'
import { instantiate } from '../wabt-wrapper.js';

{
    const instance = instantiate(`
    (func (export "foo") (param i32) (result i32)
        (local.get 0)
            (block
                (local.set 0 (i32.const 0xbbadbeef))))
    `);

    assert.eq(instance.exports.foo(3), 3);
}

{
    const instance = instantiate(`
    (func $const (result i32)
          (i32.const 42)
          )

    (func (export "foo") (param i32) (result i32 i32)
            (call $const)
            (local.get 0)
            (block (param i32) (result i32)
                        ))
    `);

    assert.eq(instance.exports.foo(3), [42, 3]);
}

{
    const instance = instantiate(`
    (func (export "foo") (param i32) (result i32)
        (local.get 0)
        (if (local.get 0)
              (then (local.set 0 (i32.const 42)))
          (else (local.set 0 (i32.const 13)))))
    `);

    assert.eq(instance.exports.foo(1), 1);
    assert.eq(instance.exports.foo(0), 0);
}
