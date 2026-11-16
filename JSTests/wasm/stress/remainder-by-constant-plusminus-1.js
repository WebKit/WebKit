import { instantiate } from "../wabt-wrapper.js";
import * as assert from "../assert.js";

let wat = `
(module
    (func (export "testI32RemU_n1") (param i32) (result i32)
        local.get 0
        i32.const -1
        i32.rem_u
    )

    (func (export "testI32RemS_n1") (param i32) (result i32)
        local.get 0
        i32.const -1
        i32.rem_s
    )

    (func (export "testI64RemU_n1") (param i64) (result i64)
        local.get 0
        i64.const -1
        i64.rem_u
    )

    (func (export "testI64RemS_n1") (param i64) (result i64)
        local.get 0
        i64.const -1
        i64.rem_s
    )

    (func (export "testI32RemU_1") (param i32) (result i32)
        local.get 0
        i32.const 1
        i32.rem_u
    )

    (func (export "testI32RemS_1") (param i32) (result i32)
        local.get 0
        i32.const 1
        i32.rem_s
    )

    (func (export "testI64RemU_1") (param i64) (result i64)
        local.get 0
        i64.const 1
        i64.rem_u
    )

    (func (export "testI64RemS_1") (param i64) (result i64)
        local.get 0
        i64.const 1
        i64.rem_s
    )
)
`;

async function test() {
    const instance = await instantiate(wat, {}, {});
    const { 
        testI32RemU_n1,
        testI32RemS_n1,
        testI64RemU_n1,
        testI64RemS_n1,
        testI32RemU_1,
        testI32RemS_1,
        testI64RemU_1,
        testI64RemS_1,
    } = instance.exports;


    /***** -1 ******/
    assert.eq(testI32RemU_n1(0), 0);
    assert.eq(testI32RemU_n1(1), 1);
    assert.eq(testI32RemU_n1(2), 2);
    assert.eq(testI32RemU_n1(7), 7);
    assert.eq(testI32RemU_n1(8), 8);
    assert.eq(testI32RemU_n1(9), 9);

    assert.eq(testI32RemS_n1(0), 0);
    assert.eq(testI32RemS_n1(1), 0);
    assert.eq(testI32RemS_n1(2), 0);
    assert.eq(testI32RemS_n1(7), 0);
    assert.eq(testI32RemS_n1(8), 0);
    assert.eq(testI32RemS_n1(9), 0);
    assert.eq(testI32RemS_n1(-1), 0);
    assert.eq(testI32RemS_n1(-2), 0);
    assert.eq(testI32RemS_n1(-7), 0);
    assert.eq(testI32RemS_n1(-8), 0);
    assert.eq(testI32RemS_n1(-9), 0);

    assert.eq(testI64RemU_n1(0n), 0n);
    assert.eq(testI64RemU_n1(1n), 1n);
    assert.eq(testI64RemU_n1(2n), 2n);
    assert.eq(testI64RemU_n1(7n), 7n);
    assert.eq(testI64RemU_n1(8n), 8n);
    assert.eq(testI64RemU_n1(9n), 9n);

    assert.eq(testI64RemS_n1(0n), 0n);
    assert.eq(testI64RemS_n1(1n), 0n);
    assert.eq(testI64RemS_n1(2n), 0n);
    assert.eq(testI64RemS_n1(7n), 0n);
    assert.eq(testI64RemS_n1(8n), 0n);
    assert.eq(testI64RemS_n1(9n), 0n);
    assert.eq(testI64RemS_n1(-1n), 0n);
    assert.eq(testI64RemS_n1(-2n), 0n);
    assert.eq(testI64RemS_n1(-7n), 0n);
    assert.eq(testI64RemS_n1(-8n), 0n);
    assert.eq(testI64RemS_n1(-9n), 0n);

    /***** +1 ******/
    assert.eq(testI32RemU_1(0), 0);
    assert.eq(testI32RemU_1(1), 0);
    assert.eq(testI32RemU_1(2), 0);
    assert.eq(testI32RemU_1(7), 0);
    assert.eq(testI32RemU_1(8), 0);
    assert.eq(testI32RemU_1(9), 0);

    assert.eq(testI32RemS_1(0), 0);
    assert.eq(testI32RemS_1(1), 0);
    assert.eq(testI32RemS_1(2), 0);
    assert.eq(testI32RemS_1(7), 0);
    assert.eq(testI32RemS_1(8), 0);
    assert.eq(testI32RemS_1(9), 0);
    assert.eq(testI32RemS_1(-1), 0);
    assert.eq(testI32RemS_1(-2), 0);
    assert.eq(testI32RemS_1(-7), 0);
    assert.eq(testI32RemS_1(-8), 0);
    assert.eq(testI32RemS_1(-9), 0);

    assert.eq(testI64RemU_1(0n), 0n);
    assert.eq(testI64RemU_1(1n), 0n);
    assert.eq(testI64RemU_1(2n), 0n);
    assert.eq(testI64RemU_1(7n), 0n);
    assert.eq(testI64RemU_1(8n), 0n);
    assert.eq(testI64RemU_1(9n), 0n);

    assert.eq(testI64RemS_1(0n), 0n);
    assert.eq(testI64RemS_1(1n), 0n);
    assert.eq(testI64RemS_1(2n), 0n);
    assert.eq(testI64RemS_1(7n), 0n);
    assert.eq(testI64RemS_1(8n), 0n);
    assert.eq(testI64RemS_1(9n), 0n);
    assert.eq(testI64RemS_1(-1n), 0n);
    assert.eq(testI64RemS_1(-2n), 0n);
    assert.eq(testI64RemS_1(-7n), 0n);
    assert.eq(testI64RemS_1(-8n), 0n);
    assert.eq(testI64RemS_1(-9n), 0n);
}

await assert.asyncTest(test());
