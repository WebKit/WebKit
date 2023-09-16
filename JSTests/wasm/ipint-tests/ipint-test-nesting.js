import { instantiate } from "../wabt-wrapper.js"
import * as assert from "../assert.js"

let wat = `
(module
    (func (export "test") (result i32)
        (block (result i32)
            (block (result i32)
                (block (result i32)
                    (block (result i32)
                        (block (result i32)
                            (block (result i32)
                                (block (result i32)
                                    (block (result i32)
                                        (block (result i32)
                                            (block (result i32)
                                                (block (result i32)
                                                    (i32.const 3)
                                                    (br 5)
                                                )
                                            )
                                        )
                                    )
                                )
                            )
                        )
                    )
                )
            )
        )
        (block (param i32) (result i32)
            (block (result i32)
                (i32.const 2)
            )
            (i32.add)
        )
        (return)
    )
    (func (export "test2") (param i32) (result i32)
        (block
            (block
                (block
                    (loop
                        (block
                            (loop
                                (block
                                    (local.get 0)
                                    (i32.const 1)
                                    (i32.add)
                                    (local.set 0)
                                )
                            )
                        )
                        (loop
                            (block
                                (nop)
                            )
                        )
                    )
                )
            )
            (loop
                (loop
                    (loop
                        (local.get 0)
                        (local.set 0)
                    )
                )
            )
        )
        (local.get 0)
        (return)
    )
)
`

async function test() {
    const instance = await instantiate(wat, {});
    const { test, test2 } = instance.exports
    assert.eq(test(), 5)
    assert.eq(test2(5), 6)
}

assert.asyncTest(test())
